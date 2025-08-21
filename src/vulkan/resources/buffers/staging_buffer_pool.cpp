#include "staging_buffer_pool.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include "../../core/vulkan_raii.h"
#include <iostream>
#include <algorithm>

StagingBufferPool::~StagingBufferPool() {
    cleanup();
}

bool StagingBufferPool::initialize(const VulkanContext& context, VkDeviceSize size) {
    this->context = &context;
    this->totalSize = size;
    this->currentOffset = 0;
    
    const auto& vk = context.getLoader();
    const VkDevice device = context.getDevice();
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (vk.vkCreateBuffer(device, &bufferInfo, nullptr, &bufferHandle) != VK_SUCCESS) {
        std::cerr << "Failed to create staging ring buffer!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vk.vkGetBufferMemoryRequirements(device, bufferHandle, &memRequirements);
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vk.vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &memProperties);
    
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
        vk.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    VkDeviceMemory memory;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    if (vk.vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging buffer memory!" << std::endl;
        vk.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    if (vk.vkBindBufferMemory(device, bufferHandle, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind staging buffer memory!" << std::endl;
        vk.vkFreeMemory(device, memory, nullptr);
        vk.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    if (vk.vkMapMemory(device, memory, 0, size, 0, &ringBuffer.mappedData) != VK_SUCCESS) {
        std::cerr << "Failed to map staging buffer memory!" << std::endl;
        vk.vkFreeMemory(device, memory, nullptr);
        vk.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    ringBuffer.buffer = vulkan_raii::make_buffer(bufferHandle, &context);
    ringBuffer.memory = vulkan_raii::make_device_memory(memory, &context);
    ringBuffer.size = size;
    
    return true;
}

void StagingBufferPool::cleanup() {
    if (context && ringBuffer.isValid()) {
        if (ringBuffer.mappedData && ringBuffer.memory) {
            context->getLoader().vkUnmapMemory(context->getDevice(), ringBuffer.memory.get());
        }
        
        ringBuffer.buffer.reset();
        ringBuffer.memory.reset();
        
        ringBuffer.mappedData = nullptr;
        ringBuffer.size = 0;
        currentOffset = 0;
        totalSize = 0;
        totalWastedBytes = 0;
        wrapAroundCount = 0;
        largestFreeBlock = 0;
    }
    context = nullptr;
}

StagingBufferPool::StagingRegion StagingBufferPool::allocate(VkDeviceSize size, VkDeviceSize alignment) {
    totalAllocations++;
    
    if (!ringBuffer.isValid() || size == 0) {
        failedAllocations++;
        return {};
    }
    
    VkDeviceSize alignedOffset = ((currentOffset + alignment - 1) / alignment) * alignment;
    VkDeviceSize wastedBytes = alignedOffset - currentOffset;
    
    if (alignedOffset + size > totalSize) {
        alignedOffset = 0;
        wastedBytes += totalSize - currentOffset;
        wrapAroundCount++;
        
        if (size > totalSize) {
            failedAllocations++;
            return {};
        }
    }
    
    totalWastedBytes += wastedBytes;
    largestFreeBlock = std::max(largestFreeBlock, totalSize - (alignedOffset + size));
    
    StagingRegion region;
    region.buffer = ringBuffer.buffer.get();
    region.mappedData = static_cast<char*>(ringBuffer.mappedData) + alignedOffset;
    region.offset = alignedOffset;
    region.size = size;
    
    currentOffset = alignedOffset + size;
    
    return region;
}

StagingBufferPool::StagingRegionGuard StagingBufferPool::allocateGuarded(VkDeviceSize size, VkDeviceSize alignment) {
    return StagingRegionGuard(this, size, alignment);
}

void StagingBufferPool::reset() {
    currentOffset = 0;
    totalWastedBytes = 0;
    largestFreeBlock = totalSize;
}

bool StagingBufferPool::tryDefragment() {
    reset();
    return true;
}

VkDeviceSize StagingBufferPool::getFragmentedBytes() const {
    return totalWastedBytes;
}

bool StagingBufferPool::isFragmentationCritical() const {
    if (totalSize == 0) return false;
    
    float fragmentationRatio = static_cast<float>(totalWastedBytes) / totalSize;
    return fragmentationRatio > 0.3f || wrapAroundCount > 10;
}

StagingBufferPool::PoolStats StagingBufferPool::getStats() const {
    PoolStats stats;
    stats.totalSize = totalSize;
    stats.fragmentedBytes = totalWastedBytes;
    stats.fragmentationRatio = totalSize > 0 ? static_cast<float>(totalWastedBytes) / totalSize : 0.0f;
    stats.fragmentationCritical = isFragmentationCritical();
    stats.allocations = totalAllocations;
    stats.failedAllocations = failedAllocations;
    return stats;
}

bool StagingBufferPool::isUnderPressure() const {
    if (totalAllocations == 0) return false;
    
    float failureRate = static_cast<float>(failedAllocations) / totalAllocations;
    return failureRate > 0.1f || isFragmentationCritical();
}