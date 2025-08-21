#include "transfer_manager.h"
#include "resource_context.h"
#include "buffer_factory.h"
#include "../core/vulkan_context.h"
#include <iostream>
#include <cstring>
#include <algorithm>

TransferManager::TransferManager() {
}

TransferManager::~TransferManager() {
    cleanup();
}

bool TransferManager::initialize(const ResourceContext* resourceContext,
                                BufferFactory* bufferFactory,
                                StagingBufferManager* stagingManager,
                                CommandExecutor* executor) {
    this->resourceContext = resourceContext;
    this->bufferFactory = bufferFactory;
    this->stagingManager = stagingManager;
    this->executor = executor;
    
    if (!resourceContext || !bufferFactory || !stagingManager) {
        std::cerr << "TransferManager: Invalid dependencies provided!" << std::endl;
        return false;
    }
    
    return true;
}

void TransferManager::cleanup() {
    // Force completion of any pending transfers
    if (executor) {
        flushPendingTransfers();
    }
    
    resourceContext = nullptr;
    bufferFactory = nullptr;
    stagingManager = nullptr;
    executor = nullptr;
    
    // Reset stats
    resetStats();
}

bool TransferManager::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
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
        updateStats(size);
    }
    
    return success;
}

bool TransferManager::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst,
                                        VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!src.isValid() || !dst.isValid() || size == 0 || !bufferFactory) {
        return false;
    }
    
    bufferFactory->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset);
    updateStats(size);
    return true;
}

CommandExecutor::AsyncTransfer TransferManager::copyToBufferAsync(const ResourceHandle& dst, const void* data,
                                                                 VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return {}; // Invalid async transfer
    }
    
    CommandExecutor::AsyncTransfer result;
    
    if (isBufferHostVisible(dst)) {
        // Direct copy to mapped buffer - no async needed
        bool success = copyDirectToMappedBuffer(dst, data, size, offset);
        if (success) {
            updateStats(size, true);
        }
        return {}; // Return empty transfer (already complete)
    } else {
        result = copyStagedToBufferAsync(dst, data, size, offset);
        if (result.isValid()) {
            updateStats(size, true);
        }
    }
    
    return result;
}

CommandExecutor::AsyncTransfer TransferManager::copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
                                                                       VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!src.isValid() || !dst.isValid() || size == 0 || !executor) {
        return {}; // Invalid async transfer
    }
    
    auto result = executor->copyBufferToBufferAsync(src.buffer.get(), dst.buffer.get(), size, srcOffset, dstOffset);
    if (result.isValid()) {
        updateStats(size, true);
    }
    
    return result;
}

bool TransferManager::executeBatch(const TransferBatch& batch) {
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
        updateStats(totalBytes, false, true);
    }
    
    return allSucceeded;
}

CommandExecutor::AsyncTransfer TransferManager::executeBatchAsync(const TransferBatch& batch) {
    if (batch.empty()) {
        return {};
    }
    
    // For now, execute batch operations sequentially
    // Future optimization: batch staging operations and single async transfer
    VkDeviceSize totalBytes = 0;
    
    for (const auto& transfer : batch.transfers) {
        if (!transfer.data || !transfer.dstBuffer || transfer.size == 0) {
            continue;
        }
        
        auto asyncResult = copyToBufferAsync(*transfer.dstBuffer, transfer.data, transfer.size, transfer.offset);
        totalBytes += transfer.size;
        
        // For simplicity, we return the last async transfer
        // In a full implementation, we'd coordinate multiple transfers
    }
    
    if (totalBytes > 0) {
        updateStats(totalBytes, true, true);
    }
    
    // Return empty transfer for batch - individual transfers are tracked separately
    return {};
}

bool TransferManager::mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid() || !bufferFactory) {
        return false;
    }
    
    // If buffer is already mapped, use direct copy
    if (dst.mappedData) {
        return copyDirectToMappedBuffer(dst, data, size, offset);
    }
    
    // Temporarily map the buffer if it's host-visible
    if (isBufferHostVisible(dst)) {
        // Use buffer factory's mapping capabilities
        return copyToBuffer(dst, data, size, offset);
    }
    
    // Buffer is not host-visible, use staging
    return copyStagedToBuffer(dst, data, size, offset);
}

TransferManager::TransferStats TransferManager::getStats() const {
    // Update average transfer size
    stats.averageTransferSize = stats.totalTransfers > 0 ? 
        (float)stats.totalBytesTransferred / stats.totalTransfers : 0.0f;
    
    // Update staging buffer usage from staging manager
    if (stagingManager) {
        auto stagingStats = stagingManager->getStats();
        stats.stagingBytesUsed = stagingStats.fragmentedBytes;
        stats.activeStagingRegions = stagingStats.allocationCount;
    }
    
    return stats;
}

void TransferManager::resetStats() {
    stats = TransferStats{};
}

bool TransferManager::tryOptimizeTransfers() {
    if (!stagingManager) {
        return false;
    }
    
    // Try to defragment staging buffers
    return stagingManager->tryDefragment();
}

bool TransferManager::isTransferQueueAvailable() const {
    return executor && executor->usesDedicatedTransferQueue();
}

void TransferManager::flushPendingTransfers() {
    // Note: We don't have a general "wait for all transfers" method
    // Individual transfers should be waited for as needed
    // This is a placeholder implementation
}

// Private helper methods
bool TransferManager::isBufferHostVisible(const ResourceHandle& buffer) const {
    return buffer.mappedData != nullptr;
}

bool TransferManager::requiresStaging(const ResourceHandle& buffer) const {
    return !isBufferHostVisible(buffer);
}

StagingRingBuffer::StagingRegion TransferManager::allocateStaging(VkDeviceSize size, VkDeviceSize alignment) {
    if (!stagingManager) {
        return {};
    }
    
    return stagingManager->allocate(size, alignment);
}

bool TransferManager::copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!dst.mappedData || !data) {
        return false;
    }
    
    memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
    return true;
}

bool TransferManager::copyStagedToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!bufferFactory) {
        return false;
    }
    
    // Delegate to buffer factory which handles staging
    bufferFactory->copyToBuffer(dst, data, size, offset);
    return true;
}

CommandExecutor::AsyncTransfer TransferManager::copyStagedToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!stagingManager || !executor) {
        return {};
    }
    
    // Allocate staging region
    auto stagingRegion = allocateStaging(size);
    if (!stagingRegion.isValid()) {
        // Try to reset and allocate again
        stagingManager->reset();
        stagingRegion = allocateStaging(size);
    }
    
    if (!stagingRegion.isValid()) {
        std::cerr << "TransferManager::copyStagedToBufferAsync: Failed to allocate staging buffer for " 
                  << size << " bytes" << std::endl;
        return {};
    }
    
    // Copy data to staging region
    memcpy(stagingRegion.mappedData, data, size);
    
    // Create async transfer from staging to destination
    auto asyncTransfer = executor->copyBufferToBufferAsync(
        stagingRegion.buffer, dst.buffer.get(), size,
        stagingRegion.offset, offset
    );
    
    return asyncTransfer;
}

void TransferManager::updateStats(VkDeviceSize bytesTransferred, bool wasAsync, bool wasBatch) {
    stats.totalTransfers++;
    stats.totalBytesTransferred += bytesTransferred;
    
    if (wasAsync) {
        stats.asyncTransfers++;
    }
    
    if (wasBatch) {
        stats.batchTransfers++;
    }
}