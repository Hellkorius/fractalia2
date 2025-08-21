#include "staging_buffer_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_constants.h"
#include "../core/vulkan_raii.h"
#include "resource_handle.h"
#include <iostream>
#include <cstring>

// StagingRingBuffer implementation
bool StagingRingBuffer::initialize(const VulkanContext& context, VkDeviceSize size) {
    this->context = &context;
    this->totalSize = size;
    this->currentOffset = 0;
    
    // Cache frequently used loader and device references for performance
    const auto& loader = context.getLoader();
    const VkDevice device = context.getDevice();
    
    // Create staging buffer - always host visible and coherent
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (loader.vkCreateBuffer(device, &bufferInfo, nullptr, &bufferHandle) != VK_SUCCESS) {
        std::cerr << "Failed to create staging ring buffer!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    loader.vkGetBufferMemoryRequirements(device, bufferHandle, &memRequirements);
    
    VkPhysicalDeviceMemoryProperties memProperties;
    loader.vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &memProperties);
    
    uint32_t memoryType = UINT32_MAX;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryType = i;
            break;
        }
    }
    
    if (memoryType == UINT32_MAX) {
        std::cerr << "Failed to find suitable memory type for staging buffer!" << std::endl;
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    VkDeviceMemory memory;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    if (loader.vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging buffer memory!" << std::endl;
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    if (loader.vkBindBufferMemory(device, bufferHandle, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind staging buffer memory!" << std::endl;
        loader.vkFreeMemory(device, memory, nullptr);
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    if (loader.vkMapMemory(device, memory, 0, size, 0, &ringBuffer.mappedData) != VK_SUCCESS) {
        std::cerr << "Failed to map staging buffer memory!" << std::endl;
        loader.vkFreeMemory(device, memory, nullptr);
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    // Wrap handles in RAII wrappers
    ringBuffer.buffer = vulkan_raii::make_buffer(bufferHandle, &context);
    ringBuffer.memory = vulkan_raii::make_device_memory(memory, &context);
    ringBuffer.size = size;
    
    return true;
}

void StagingRingBuffer::cleanup() {
    if (context && ringBuffer.isValid()) {
        if (ringBuffer.mappedData && ringBuffer.memory) {
            context->getLoader().vkUnmapMemory(context->getDevice(), ringBuffer.memory.get());
        }
        
        // RAII wrappers will handle cleanup automatically
        ringBuffer.buffer.reset();
        ringBuffer.memory.reset();
        
        ringBuffer.mappedData = nullptr;
        ringBuffer.size = 0;
        currentOffset = 0;
        totalSize = 0;
    }
}

StagingRingBuffer::StagingRegion StagingRingBuffer::allocate(VkDeviceSize size, VkDeviceSize alignment) {
    // Align current offset
    VkDeviceSize alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
    VkDeviceSize wastedBytes = alignedOffset - currentOffset;
    
    // Check if we need to wrap around
    if (alignedOffset + size > totalSize) {
        // Track fragmentation from wrap-around
        totalWastedBytes += (totalSize - currentOffset);
        wrapAroundCount++;
        
        alignedOffset = 0; // Wrap to beginning
        currentOffset = 0;
        wastedBytes = 0; // No alignment waste after wrap
    }
    
    if (alignedOffset + size > totalSize) {
        std::cerr << "Staging buffer allocation too large: " << size << " bytes" << std::endl;
        return {};
    }
    
    // Track alignment waste
    totalWastedBytes += wastedBytes;
    
    StagingRegion region;
    region.buffer = ringBuffer.buffer.get();
    region.offset = alignedOffset;
    region.size = size;
    region.mappedData = static_cast<char*>(ringBuffer.mappedData) + alignedOffset;
    
    currentOffset = alignedOffset + size;
    
    // Update largest free block tracking
    largestFreeBlock = totalSize - currentOffset;
    
    return region;
}

StagingRingBuffer::StagingRegionGuard StagingRingBuffer::allocateGuarded(VkDeviceSize size, VkDeviceSize alignment) {
    return StagingRegionGuard(this, size, alignment);
}

void StagingRingBuffer::reset() {
    currentOffset = 0;
    // Reset fragmentation tracking on full reset
    totalWastedBytes = 0;
    wrapAroundCount = 0;
    largestFreeBlock = totalSize;
}

bool StagingRingBuffer::tryDefragment() {
    // For ring buffers, defragmentation means forced reset if fragmentation is critical
    if (isFragmentationCritical()) {
        reset();
        return true;
    }
    return false;
}

VkDeviceSize StagingRingBuffer::getFragmentedBytes() const {
    return totalWastedBytes;
}

bool StagingRingBuffer::isFragmentationCritical() const {
    if (totalSize == 0) return false;
    float fragmentationRatio = (float)totalWastedBytes / totalSize;
    return fragmentationRatio > 0.5f; // >50% fragmented is critical
}

// StagingBufferManager implementation
StagingBufferManager::StagingBufferManager() {
}

StagingBufferManager::~StagingBufferManager() {
    cleanup();
}

bool StagingBufferManager::initialize(const VulkanContext& context, VkDeviceSize initialSize) {
    this->context = &context;
    this->initialBufferSize = initialSize;
    
    // Initialize primary staging buffer
    if (!primaryBuffer.initialize(context, initialSize)) {
        std::cerr << "Failed to initialize primary staging buffer!" << std::endl;
        return false;
    }
    
    return true;
}

void StagingBufferManager::cleanup() {
    primaryBuffer.cleanup();
    context = nullptr;
    totalAllocations = 0;
    failedAllocations = 0;
}

StagingRingBuffer::StagingRegion StagingBufferManager::allocate(VkDeviceSize size, VkDeviceSize alignment) {
    totalAllocations++;
    
    auto region = primaryBuffer.allocate(size, alignment);
    if (!region.isValid()) {
        failedAllocations++;
        
        // Try defragmenting and allocate again
        if (primaryBuffer.tryDefragment()) {
            region = primaryBuffer.allocate(size, alignment);
            if (!region.isValid()) {
                failedAllocations++;
            }
        }
    }
    
    return region;
}

StagingRingBuffer::StagingRegionGuard StagingBufferManager::allocateGuarded(VkDeviceSize size, VkDeviceSize alignment) {
    totalAllocations++;
    
    auto guard = primaryBuffer.allocateGuarded(size, alignment);
    if (!guard.isValid()) {
        failedAllocations++;
        
        // Try defragmenting and allocate again
        if (primaryBuffer.tryDefragment()) {
            guard = primaryBuffer.allocateGuarded(size, alignment);
            if (!guard.isValid()) {
                failedAllocations++;
            }
        }
    }
    
    return guard;
}

void StagingBufferManager::reset() {
    primaryBuffer.reset();
}

bool StagingBufferManager::tryDefragment() {
    return primaryBuffer.tryDefragment();
}

StagingBufferManager::StagingStats StagingBufferManager::getStats() const {
    StagingStats stats;
    stats.totalSize = initialBufferSize;
    stats.fragmentedBytes = primaryBuffer.getFragmentedBytes();
    stats.allocationCount = totalAllocations;
    stats.fragmentationRatio = initialBufferSize > 0 ? 
        (float)stats.fragmentedBytes / initialBufferSize : 0.0f;
    stats.isFragmentationCritical = primaryBuffer.isFragmentationCritical();
    
    return stats;
}

bool StagingBufferManager::isUnderMemoryPressure() const {
    if (totalAllocations == 0) return false;
    
    float failureRate = (float)failedAllocations / totalAllocations;
    return failureRate > 0.1f || primaryBuffer.isFragmentationCritical();
}