#include "buffer_manager.h"
#include "resource_context.h"
#include "buffer_factory.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_raii.h"
#include <iostream>
#include <cstring>
#include <algorithm>

// StagingRingBuffer implementation
bool StagingRingBuffer::initialize(const VulkanContext& context, VkDeviceSize size) {
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

void StagingRingBuffer::cleanup() {
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
    }
}

StagingRingBuffer::StagingRegion StagingRingBuffer::allocate(VkDeviceSize size, VkDeviceSize alignment) {
    VkDeviceSize alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
    VkDeviceSize wastedBytes = alignedOffset - currentOffset;
    
    if (alignedOffset + size > totalSize) {
        totalWastedBytes += (totalSize - currentOffset);
        wrapAroundCount++;
        
        alignedOffset = 0;
        currentOffset = 0;
        wastedBytes = 0;
    }
    
    if (alignedOffset + size > totalSize) {
        std::cerr << "Staging buffer allocation too large: " << size << " bytes" << std::endl;
        return {};
    }
    
    totalWastedBytes += wastedBytes;
    
    StagingRegion region;
    region.buffer = ringBuffer.buffer.get();
    region.offset = alignedOffset;
    region.size = size;
    region.mappedData = static_cast<char*>(ringBuffer.mappedData) + alignedOffset;
    
    currentOffset = alignedOffset + size;
    largestFreeBlock = totalSize - currentOffset;
    
    return region;
}

StagingRingBuffer::StagingRegionGuard StagingRingBuffer::allocateGuarded(VkDeviceSize size, VkDeviceSize alignment) {
    return StagingRegionGuard(this, size, alignment);
}

void StagingRingBuffer::reset() {
    currentOffset = 0;
    totalWastedBytes = 0;
    wrapAroundCount = 0;
    largestFreeBlock = totalSize;
}

bool StagingRingBuffer::tryDefragment() {
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
    return fragmentationRatio > 0.5f;
}

// GPUBuffer implementation
GPUBuffer::~GPUBuffer() {
    cleanup();
}

bool GPUBuffer::initialize(ResourceContext* resourceContext, BufferManager* bufferManager, VkDeviceSize size,
                          VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    this->resourceContext = resourceContext;
    this->bufferManager = bufferManager;
    this->bufferSize = size;
    this->isDeviceLocal = (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    
    if (isDeviceLocal) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createMappedBuffer(size, usage, properties)
        );
    } else {
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

void GPUBuffer::cleanup() {
    if (storageHandle && resourceContext) {
        resourceContext->destroyResource(*storageHandle);
        storageHandle.reset();
    }
    
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

bool GPUBuffer::addData(const void* data, VkDeviceSize size, VkDeviceSize alignment) {
    if (!storageHandle || !data) return false;
    
    if (storageHandle->mappedData) {
        memcpy(static_cast<char*>(storageHandle->mappedData) + stagingBytesWritten, data, size);
        stagingBytesWritten += size;
        return true;
    }
    
    if (!isDeviceLocal || !resourceContext) return false;
    
    if (!bufferManager) return false;
    
    auto stagingRegion = bufferManager->allocateStaging(size, alignment);
    
    if (!stagingRegion.mappedData) {
        bufferManager->resetAllStaging();
        stagingBytesWritten = 0;
        stagingStartOffset = 0;
        stagingRegion = bufferManager->allocateStaging(size, alignment);
    }
    
    if (stagingRegion.mappedData) {
        memcpy(stagingRegion.mappedData, data, size);
        if (stagingBytesWritten == 0) {
            stagingStartOffset = stagingRegion.offset;
        }
        stagingBytesWritten += size;
        needsUpload = true;
        return true;
    }
    
    return false;
}

void GPUBuffer::flushToGPU(VkDeviceSize dstOffset) {
    if (!needsUpload || stagingBytesWritten == 0 || !isDeviceLocal) {
        return;
    }
    
    if (!bufferManager) return;
    
    auto& stagingBuffer = bufferManager->getPrimaryStagingBuffer();
    
    ResourceHandle stagingHandle;
    stagingHandle.buffer = vulkan_raii::make_buffer(stagingBuffer.getBuffer(), resourceContext->getContext());
    stagingHandle.buffer.detach();
    
    resourceContext->copyBufferToBuffer(
        stagingHandle,
        *storageHandle,
        stagingBytesWritten,
        stagingStartOffset,
        dstOffset
    );
    
    resetStaging();
}

void GPUBuffer::resetStaging() {
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

// BufferManager implementation
BufferManager::BufferManager() {
}

BufferManager::~BufferManager() {
    cleanup();
}

bool BufferManager::initialize(const ResourceContext* resourceContext, 
                              BufferFactory* bufferFactory,
                              CommandExecutor* executor,
                              VkDeviceSize stagingSize) {
    this->resourceContext = resourceContext;
    this->bufferFactory = bufferFactory;
    this->executor = executor;
    
    if (!resourceContext || !bufferFactory) {
        std::cerr << "BufferManager: Invalid dependencies provided!" << std::endl;
        return false;
    }
    
    if (!primaryStagingBuffer.initialize(*resourceContext->getContext(), stagingSize)) {
        std::cerr << "Failed to initialize primary staging buffer!" << std::endl;
        return false;
    }
    
    return true;
}

void BufferManager::cleanup() {
    managedBuffers.clear();
    primaryStagingBuffer.cleanup();
    
    resourceContext = nullptr;
    bufferFactory = nullptr;
    executor = nullptr;
    
    totalStagingAllocations = 0;
    failedStagingAllocations = 0;
    transferStats = {};
}

StagingRingBuffer::StagingRegion BufferManager::allocateStaging(VkDeviceSize size, VkDeviceSize alignment) {
    totalStagingAllocations++;
    
    auto region = primaryStagingBuffer.allocate(size, alignment);
    if (!region.isValid()) {
        failedStagingAllocations++;
        
        if (primaryStagingBuffer.tryDefragment()) {
            region = primaryStagingBuffer.allocate(size, alignment);
            if (!region.isValid()) {
                failedStagingAllocations++;
            }
        }
    }
    
    return region;
}

StagingRingBuffer::StagingRegionGuard BufferManager::allocateStagingGuarded(VkDeviceSize size, VkDeviceSize alignment) {
    totalStagingAllocations++;
    
    auto guard = primaryStagingBuffer.allocateGuarded(size, alignment);
    if (!guard.isValid()) {
        failedStagingAllocations++;
        
        if (primaryStagingBuffer.tryDefragment()) {
            guard = primaryStagingBuffer.allocateGuarded(size, alignment);
            if (!guard.isValid()) {
                failedStagingAllocations++;
            }
        }
    }
    
    return guard;
}

void BufferManager::resetAllStaging() {
    primaryStagingBuffer.reset();
}

std::unique_ptr<GPUBuffer> BufferManager::createBuffer(VkDeviceSize size,
                                                      VkBufferUsageFlags usage,
                                                      VkMemoryPropertyFlags properties) {
    auto buffer = std::make_unique<GPUBuffer>();
    
    if (!buffer->initialize(const_cast<ResourceContext*>(resourceContext), this, size, usage, properties)) {
        return nullptr;
    }
    
    registerBuffer(buffer.get());
    
    return buffer;
}

bool BufferManager::uploadData(GPUBuffer& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (buffer.getMappedData()) {
        memcpy(static_cast<char*>(buffer.getMappedData()) + offset, data, size);
        return true;
    }
    
    return buffer.addData(data, size);
}

void BufferManager::flushAllBuffers() {
    for (auto* buffer : managedBuffers) {
        if (buffer && buffer->hasPendingData()) {
            buffer->flushToGPU();
        }
    }
}

bool BufferManager::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return false;
    }
    
    bool success = false;
    
    if (isBufferHostVisible(dst)) {
        success = copyDirectToMappedBuffer(dst, data, size, offset);
    } else {
        success = copyStagedToBuffer(dst, data, size, offset);
    }
    
    if (success) {
        updateTransferStats(size);
    }
    
    return success;
}

bool BufferManager::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst,
                                      VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!src.isValid() || !dst.isValid() || size == 0 || !bufferFactory) {
        return false;
    }
    
    bufferFactory->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset);
    updateTransferStats(size);
    return true;
}

CommandExecutor::AsyncTransfer BufferManager::copyToBufferAsync(const ResourceHandle& dst, const void* data,
                                                               VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return {};
    }
    
    CommandExecutor::AsyncTransfer result;
    
    if (isBufferHostVisible(dst)) {
        bool success = copyDirectToMappedBuffer(dst, data, size, offset);
        if (success) {
            updateTransferStats(size, true);
        }
        return {};
    } else {
        result = copyStagedToBufferAsync(dst, data, size, offset);
        if (result.isValid()) {
            updateTransferStats(size, true);
        }
    }
    
    return result;
}

CommandExecutor::AsyncTransfer BufferManager::copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
                                                                     VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!src.isValid() || !dst.isValid() || size == 0 || !executor) {
        return {};
    }
    
    auto result = executor->copyBufferToBufferAsync(src.buffer.get(), dst.buffer.get(), size, srcOffset, dstOffset);
    if (result.isValid()) {
        updateTransferStats(size, true);
    }
    
    return result;
}

bool BufferManager::executeBatch(const TransferBatch& batch) {
    if (batch.empty()) {
        return true;
    }
    
    bool allSucceeded = true;
    VkDeviceSize totalBytes = 0;
    
    for (const auto& transfer : batch.transfers) {
        if (!transfer.data || !transfer.dstBuffer || transfer.size == 0) {
            allSucceeded = false;
            continue;
        }
        
        bool success = copyToBuffer(*transfer.dstBuffer, transfer.data, transfer.size, transfer.offset);
        if (!success) {
            allSucceeded = false;
        } else {
            totalBytes += transfer.size;
        }
    }
    
    if (totalBytes > 0) {
        updateTransferStats(totalBytes, false, true);
    }
    
    return allSucceeded;
}

CommandExecutor::AsyncTransfer BufferManager::executeBatchAsync(const TransferBatch& batch) {
    if (batch.empty()) {
        return {};
    }
    
    VkDeviceSize totalBytes = 0;
    
    for (const auto& transfer : batch.transfers) {
        if (!transfer.data || !transfer.dstBuffer || transfer.size == 0) {
            continue;
        }
        
        auto asyncResult = copyToBufferAsync(*transfer.dstBuffer, transfer.data, transfer.size, transfer.offset);
        totalBytes += transfer.size;
    }
    
    if (totalBytes > 0) {
        updateTransferStats(totalBytes, true, true);
    }
    
    return {};
}

bool BufferManager::mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return false;
    }
    
    if (dst.mappedData) {
        return copyDirectToMappedBuffer(dst, data, size, offset);
    }
    
    if (isBufferHostVisible(dst)) {
        return copyToBuffer(dst, data, size, offset);
    }
    
    return copyStagedToBuffer(dst, data, size, offset);
}

bool BufferManager::tryOptimizeMemory() {
    return primaryStagingBuffer.tryDefragment();
}

bool BufferManager::isTransferQueueAvailable() const {
    return executor && executor->usesDedicatedTransferQueue();
}

void BufferManager::flushPendingTransfers() {
    // Placeholder - individual transfers should be waited for as needed
}

BufferManager::BufferStats BufferManager::getStats() const {
    BufferStats stats;
    
    // Staging buffer stats
    stats.stagingTotalSize = primaryStagingBuffer.getTotalSize();
    stats.stagingFragmentedBytes = primaryStagingBuffer.getFragmentedBytes();
    stats.stagingFragmentationRatio = stats.stagingTotalSize > 0 ? 
        (float)stats.stagingFragmentedBytes / stats.stagingTotalSize : 0.0f;
    stats.stagingFragmentationCritical = primaryStagingBuffer.isFragmentationCritical();
    stats.stagingAllocations = totalStagingAllocations;
    
    // GPU buffer stats
    for (const auto* buffer : managedBuffers) {
        if (!buffer) continue;
        
        stats.totalBuffers++;
        stats.totalBufferSize += buffer->getSize();
        
        if (buffer->getMappedData()) {
            stats.hostVisibleBuffers++;
        } else {
            stats.deviceLocalBuffers++;
            
            if (buffer->hasPendingData()) {
                stats.buffersWithPendingData++;
            }
        }
    }
    
    // Transfer stats
    stats.totalTransfers = transferStats.totalTransfers;
    stats.asyncTransfers = transferStats.asyncTransfers;
    stats.batchTransfers = transferStats.batchTransfers;
    stats.totalBytesTransferred = transferStats.totalBytesTransferred;
    stats.averageTransferSize = stats.totalTransfers > 0 ? 
        (float)stats.totalBytesTransferred / stats.totalTransfers : 0.0f;
    
    return stats;
}

bool BufferManager::isUnderMemoryPressure() const {
    if (totalStagingAllocations == 0) return false;
    
    float failureRate = (float)failedStagingAllocations / totalStagingAllocations;
    return failureRate > 0.1f || primaryStagingBuffer.isFragmentationCritical();
}

bool BufferManager::hasPendingStagingOperations() const {
    for (const auto* buffer : managedBuffers) {
        if (buffer && buffer->hasPendingData()) {
            return true;
        }
    }
    return false;
}

// Private helper methods
bool BufferManager::isBufferHostVisible(const ResourceHandle& buffer) const {
    return buffer.mappedData != nullptr;
}

bool BufferManager::requiresStaging(const ResourceHandle& buffer) const {
    return !isBufferHostVisible(buffer);
}

bool BufferManager::copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!dst.mappedData || !data) {
        return false;
    }
    
    memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
    return true;
}

bool BufferManager::copyStagedToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!bufferFactory) {
        return false;
    }
    
    bufferFactory->copyToBuffer(dst, data, size, offset);
    return true;
}

CommandExecutor::AsyncTransfer BufferManager::copyStagedToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!executor) {
        return {};
    }
    
    auto stagingRegion = allocateStaging(size);
    if (!stagingRegion.isValid()) {
        resetAllStaging();
        stagingRegion = allocateStaging(size);
    }
    
    if (!stagingRegion.isValid()) {
        std::cerr << "BufferManager::copyStagedToBufferAsync: Failed to allocate staging buffer for " 
                  << size << " bytes" << std::endl;
        return {};
    }
    
    memcpy(stagingRegion.mappedData, data, size);
    
    auto asyncTransfer = executor->copyBufferToBufferAsync(
        stagingRegion.buffer, dst.buffer.get(), size,
        stagingRegion.offset, offset
    );
    
    return asyncTransfer;
}

void BufferManager::registerBuffer(GPUBuffer* buffer) {
    if (buffer) {
        managedBuffers.push_back(buffer);
    }
}

void BufferManager::unregisterBuffer(GPUBuffer* buffer) {
    managedBuffers.erase(
        std::remove(managedBuffers.begin(), managedBuffers.end(), buffer),
        managedBuffers.end()
    );
}

void BufferManager::updateTransferStats(VkDeviceSize bytesTransferred, bool wasAsync, bool wasBatch) {
    transferStats.totalTransfers++;
    transferStats.totalBytesTransferred += bytesTransferred;
    
    if (wasAsync) {
        transferStats.asyncTransfers++;
    }
    
    if (wasBatch) {
        transferStats.batchTransfers++;
    }
}