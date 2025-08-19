#include "resource_context.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include "../../PolygonFactory.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <array>

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

// GPUBufferRing implementation
GPUBufferRing::~GPUBufferRing() {
    cleanup();
}

bool GPUBufferRing::initialize(ResourceContext* resourceContext, VkDeviceSize size,
                               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    this->resourceContext = resourceContext;
    this->bufferSize = size;
    this->isDeviceLocal = (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    
    // For device-local buffers, add transfer destination flag
    if (isDeviceLocal) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    
    // Create the buffer based on memory properties
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        // Host-visible buffer (can be mapped)
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createMappedBuffer(size, usage, properties)
        );
    } else {
        // Device-local buffer (cannot be mapped directly)
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createBuffer(size, usage, properties)
        );
    }
    
    if (!storageHandle->isValid()) {
        storageHandle.reset();
        return false;
    }
    
    return true;
}

void GPUBufferRing::cleanup() {
    if (storageHandle && resourceContext) {
        resourceContext->destroyResource(*storageHandle);
        storageHandle.reset();
    }
    
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

bool GPUBufferRing::addData(const void* data, VkDeviceSize size, VkDeviceSize alignment) {
    if (!storageHandle || !data) return false;
    
    // If buffer is host-visible, write directly
    if (storageHandle->mappedData) {
        memcpy(static_cast<char*>(storageHandle->mappedData) + stagingBytesWritten, data, size);
        stagingBytesWritten += size;
        return true;
    }
    
    // For device-local buffers, use staging buffer
    if (!isDeviceLocal || !resourceContext) return false;
    
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    auto stagingRegion = stagingBuffer.allocate(size, alignment);
    
    if (!stagingRegion.mappedData) {
        // Reset and try again
        stagingBuffer.reset();
        stagingBytesWritten = 0;
        stagingStartOffset = 0;
        stagingRegion = stagingBuffer.allocate(size, alignment);
    }
    
    if (stagingRegion.mappedData) {
        memcpy(stagingRegion.mappedData, data, size);
        if (stagingBytesWritten == 0) {
            // Track where our batch starts
            stagingStartOffset = stagingRegion.offset;
        }
        stagingBytesWritten += size;
        needsUpload = true;
        return true;
    }
    
    return false;
}

void GPUBufferRing::flushToGPU(VkDeviceSize dstOffset) {
    if (!needsUpload || stagingBytesWritten == 0 || !isDeviceLocal) {
        return;
    }
    
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    
    // Create a temporary ResourceHandle that doesn't own the buffer
    ResourceHandle stagingHandle;
    stagingHandle.buffer = vulkan_raii::make_buffer(stagingBuffer.getBuffer(), resourceContext->getContext());
    stagingHandle.buffer.detach(); // Release ownership - staging buffer manages its own lifecycle
    
    // Copy from staging buffer to device-local buffer
    resourceContext->copyBufferToBuffer(
        stagingHandle,
        *storageHandle,
        stagingBytesWritten,
        stagingStartOffset,
        dstOffset
    );
    
    // Reset staging state
    resetStaging();
}

void GPUBufferRing::resetStaging() {
    if (resourceContext) {
        resourceContext->getStagingBuffer().reset();
    }
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

// ResourceContext implementation
ResourceContext::ResourceContext() {
}

ResourceContext::~ResourceContext() {
    cleanup();
}

bool ResourceContext::initialize(const VulkanContext& context, VkCommandPool commandPool) {
    this->context = &context;
    
    // Initialize specialized managers
    memoryAllocator = std::make_unique<MemoryAllocator>();
    if (!memoryAllocator->initialize(context)) {
        std::cerr << "Failed to initialize memory allocator!" << std::endl;
        return false;
    }
    
    bufferFactory = std::make_unique<BufferFactory>();
    if (!bufferFactory->initialize(context, memoryAllocator.get())) {
        std::cerr << "Failed to initialize buffer factory!" << std::endl;
        return false;
    }
    
    descriptorPoolManager = std::make_unique<DescriptorPoolManager>();
    if (!descriptorPoolManager->initialize(context)) {
        std::cerr << "Failed to initialize descriptor pool manager!" << std::endl;
        return false;
    }
    
    graphicsResourceManager = std::make_unique<GraphicsResourceManager>();
    if (!graphicsResourceManager->initialize(context, bufferFactory.get())) {
        std::cerr << "Failed to initialize graphics resource manager!" << std::endl;
        return false;
    }
    
    // Initialize staging buffer (16MB for large entity uploads)
    if (!stagingBuffer.initialize(context, 16 * 1024 * 1024)) {
        std::cerr << "Failed to initialize staging buffer!" << std::endl;
        return false;
    }
    
    // Initialize command executor if command pool is provided
    if (commandPool != VK_NULL_HANDLE) {
        if (!executor.initialize(context, commandPool)) {
            std::cerr << "Failed to initialize command executor!" << std::endl;
            return false;
        }
    }
    
    // Wire up dependencies
    bufferFactory->setStagingBuffer(&stagingBuffer);
    bufferFactory->setCommandExecutor(&executor);
    
    return true;
}

bool ResourceContext::updateCommandPool(VkCommandPool newCommandPool) {
    if (!context) {
        std::cerr << "ResourceContext: Cannot update command pool - not initialized" << std::endl;
        return false;
    }
    
    std::cout << "ResourceContext: Updating command pool without full reinitialization" << std::endl;
    
    // Clean up old command executor
    executor.cleanup();
    
    // Initialize with new command pool
    if (newCommandPool != VK_NULL_HANDLE) {
        if (!executor.initialize(*context, newCommandPool)) {
            std::cerr << "ResourceContext: Failed to reinitialize command executor with new command pool!" << std::endl;
            return false;
        }
        
        // Re-wire up dependencies
        bufferFactory->setCommandExecutor(&executor);
    }
    
    std::cout << "ResourceContext: Successfully updated command pool" << std::endl;
    return true;
}

void ResourceContext::cleanup() {
    cleanupBeforeContextDestruction();
    
    // Run cleanup callbacks in reverse order
    for (auto it = cleanupCallbacks.rbegin(); it != cleanupCallbacks.rend(); ++it) {
        (*it)();
    }
    cleanupCallbacks.clear();
    
    executor.cleanup();
    stagingBuffer.cleanup();
    
    // Cleanup specialized managers
    graphicsResourceManager.reset();
    descriptorPoolManager.reset();
    bufferFactory.reset();
    memoryAllocator.reset();
    
    context = nullptr;
}

void ResourceContext::cleanupBeforeContextDestruction() {
    // Delegate to graphics resource manager
    if (graphicsResourceManager) {
        graphicsResourceManager->cleanupBeforeContextDestruction();
    }
}

ResourceHandle ResourceContext::createBuffer(VkDeviceSize size, 
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties) {
    return bufferFactory->createBuffer(size, usage, properties);
}

ResourceHandle ResourceContext::createMappedBuffer(VkDeviceSize size,
                                                  VkBufferUsageFlags usage,
                                                  VkMemoryPropertyFlags properties) {
    return bufferFactory->createMappedBuffer(size, usage, properties);
}

ResourceHandle ResourceContext::createImage(uint32_t width, uint32_t height,
                                           VkFormat format,
                                           VkImageUsageFlags usage,
                                           VkMemoryPropertyFlags properties,
                                           VkSampleCountFlagBits samples) {
    return bufferFactory->createImage(width, height, format, usage, properties, samples);
}

ResourceHandle ResourceContext::createImageView(const ResourceHandle& imageHandle,
                                               VkFormat format,
                                               VkImageAspectFlags aspectFlags) {
    return bufferFactory->createImageView(imageHandle, format, aspectFlags);
}

void ResourceContext::destroyResource(ResourceHandle& handle) {
    bufferFactory->destroyResource(handle);
}

void ResourceContext::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    bufferFactory->copyToBuffer(dst, data, size, offset);
}

void ResourceContext::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    bufferFactory->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset);
}


vulkan_raii::DescriptorPool ResourceContext::createDescriptorPool() {
    return descriptorPoolManager->createDescriptorPool();
}

vulkan_raii::DescriptorPool ResourceContext::createDescriptorPool(const DescriptorPoolConfig& config) {
    return descriptorPoolManager->createDescriptorPool(config);
}

void ResourceContext::destroyDescriptorPool(VkDescriptorPool pool) {
    descriptorPoolManager->destroyDescriptorPool(pool);
}

// Memory management methods moved to MemoryAllocator
// These methods are now delegated to specialized managers

// Graphics resource creation (delegated to GraphicsResourceManager)
bool ResourceContext::createUniformBuffers() {
    return graphicsResourceManager->createUniformBuffers();
}

bool ResourceContext::createTriangleBuffers() {
    return graphicsResourceManager->createTriangleBuffers();
}

bool ResourceContext::createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    return graphicsResourceManager->createGraphicsDescriptorPool(descriptorSetLayout);
}

bool ResourceContext::createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    return graphicsResourceManager->createGraphicsDescriptorSets(descriptorSetLayout);
}

bool ResourceContext::updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer) {
    return graphicsResourceManager->updateDescriptorSetsWithPositionBuffer(positionBuffer);
}

bool ResourceContext::updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer) {
    return graphicsResourceManager->updateDescriptorSetsWithPositionBuffers(currentPositionBuffer, targetPositionBuffer);
}

bool ResourceContext::updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    return graphicsResourceManager->updateDescriptorSetsWithEntityAndPositionBuffers(entityBuffer, positionBuffer);
}

bool ResourceContext::recreateGraphicsDescriptors() {
    return graphicsResourceManager->recreateGraphicsDescriptors();
}

// Getters for graphics resources (delegated to GraphicsResourceManager)
const std::vector<VkBuffer>& ResourceContext::getUniformBuffers() const {
    return graphicsResourceManager->getUniformBuffers();
}

const std::vector<void*>& ResourceContext::getUniformBuffersMapped() const {
    return graphicsResourceManager->getUniformBuffersMapped();
}

VkBuffer ResourceContext::getVertexBuffer() const {
    return graphicsResourceManager->getVertexBuffer();
}

VkBuffer ResourceContext::getIndexBuffer() const {
    return graphicsResourceManager->getIndexBuffer();
}

uint32_t ResourceContext::getIndexCount() const {
    return graphicsResourceManager->getIndexCount();
}

VkDescriptorPool ResourceContext::getGraphicsDescriptorPool() const {
    return graphicsResourceManager->getGraphicsDescriptorPool();
}

const std::vector<VkDescriptorSet>& ResourceContext::getGraphicsDescriptorSets() const {
    return graphicsResourceManager->getGraphicsDescriptorSets();
}

// Memory stats (delegated to MemoryAllocator)
ResourceContext::MemoryStats ResourceContext::getMemoryStats() const {
    return memoryAllocator->getMemoryStats();
}

// Async transfer (partial delegation - still needs staging buffer access)
CommandExecutor::AsyncTransfer ResourceContext::copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (dst.mappedData) {
        // Direct copy to mapped buffer - no async needed
        memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
        return {}; // Return empty transfer (already complete)
    } else {
        // Use staging buffer with async transfer
        auto stagingRegion = stagingBuffer.allocate(size);
        if (!stagingRegion.mappedData) {
            // Try to reset and allocate again
            stagingBuffer.reset();
            stagingRegion = stagingBuffer.allocate(size);
        }
        
        if (stagingRegion.mappedData) {
            memcpy(stagingRegion.mappedData, data, size);
            
            // Important: Create proper AsyncTransfer with staging region tracking
            auto asyncTransfer = executor.copyBufferToBufferAsync(
                stagingRegion.buffer, dst.buffer.get(), size, 
                stagingRegion.offset, offset
            );
            
            // Note: staging region will be valid until next stagingBuffer.reset()
            // The transfer must complete before the next reset cycle
            return asyncTransfer;
        } else {
            std::cerr << "ResourceContext::copyToBufferAsync: Failed to allocate staging buffer for " 
                      << size << " bytes" << std::endl;
        }
    }
    
    return {}; // Failed allocation
}