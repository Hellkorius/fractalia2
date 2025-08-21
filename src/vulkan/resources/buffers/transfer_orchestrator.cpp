#include "transfer_orchestrator.h"
#include "staging_buffer_pool.h"
#include "buffer_registry.h"
#include "gpu_buffer.h"
#include "buffer_factory.h"
#include "../managers/resource_context.h"
#include "../core/validation_utils.h"
#include "../core/buffer_operation_utils.h"
#include "../../core/vulkan_raii.h"
#include <cstring>

bool TransferOrchestrator::initialize(StagingBufferPool* stagingPool, 
                                     BufferRegistry* bufferRegistry,
                                     CommandExecutor* executor) {
    if (!stagingPool || !bufferRegistry || !executor) {
        return false;
    }
    
    this->stagingPool = stagingPool;
    this->bufferRegistry = bufferRegistry;
    this->executor = executor;
    
    return true;
}

void TransferOrchestrator::cleanup() {
    stagingPool = nullptr;
    bufferRegistry = nullptr;
    executor = nullptr;
    transferStats = {};
}

bool TransferOrchestrator::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return false;
    }
    
    bool success = false;
    
    if (BufferOperationUtils::isBufferHostVisible(dst)) {
        success = BufferOperationUtils::copyDirectToMappedBuffer(dst, data, size, offset);
    } else {
        success = copyStagedToBuffer(dst, data, size, offset);
    }
    
    if (success) {
        updateTransferStats(size);
    }
    
    return success;
}

bool TransferOrchestrator::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst,
                                             VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    // Use centralized buffer operation utilities
    if (!BufferOperationUtils::copyBufferToBuffer(executor, src, dst, size, srcOffset, dstOffset)) {
        return false;
    }
    
    updateTransferStats(size);
    return true;
}

CommandExecutor::AsyncTransfer TransferOrchestrator::copyToBufferAsync(const ResourceHandle& dst, const void* data,
                                                                      VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return {};
    }
    
    CommandExecutor::AsyncTransfer result;
    
    if (BufferOperationUtils::isBufferHostVisible(dst)) {
        bool success = BufferOperationUtils::copyDirectToMappedBuffer(dst, data, size, offset);
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

CommandExecutor::AsyncTransfer TransferOrchestrator::copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
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

bool TransferOrchestrator::executeBatch(const TransferBatch& batch) {
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

CommandExecutor::AsyncTransfer TransferOrchestrator::executeBatchAsync(const TransferBatch& batch) {
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

bool TransferOrchestrator::mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0 || !dst.isValid()) {
        return false;
    }
    
    if (dst.mappedData) {
        return BufferOperationUtils::copyDirectToMappedBuffer(dst, data, size, offset);
    }
    
    if (BufferOperationUtils::isBufferHostVisible(dst)) {
        return copyToBuffer(dst, data, size, offset);
    }
    
    return copyStagedToBuffer(dst, data, size, offset);
}

void TransferOrchestrator::flushAllBuffers() {
    auto pendingBuffers = bufferRegistry->getBuffersWithPendingData();
    for (auto* buffer : pendingBuffers) {
        if (buffer && buffer->hasPendingData()) {
            buffer->flushToGPU();
        }
    }
}

bool TransferOrchestrator::isTransferQueueAvailable() const {
    return executor && executor->usesDedicatedTransferQueue();
}

void TransferOrchestrator::flushPendingTransfers() {
    // Placeholder - individual transfers should be waited for as needed
}

TransferOrchestrator::TransferStats TransferOrchestrator::getStats() const {
    TransferStats stats;
    stats.totalTransfers = transferStats.totalTransfers;
    stats.asyncTransfers = transferStats.asyncTransfers;
    stats.batchTransfers = transferStats.batchTransfers;
    stats.totalBytesTransferred = transferStats.totalBytesTransferred;
    stats.averageTransferSize = stats.totalTransfers > 0 ? 
        static_cast<float>(stats.totalBytesTransferred) / stats.totalTransfers : 0.0f;
    return stats;
}

bool TransferOrchestrator::requiresStaging(const ResourceHandle& buffer) const {
    return BufferOperationUtils::requiresStaging(buffer);
}

bool TransferOrchestrator::copyStagedToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!stagingPool || !data || size == 0 || !dst.isValid()) {
        return false;
    }
    
    auto stagingRegion = stagingPool->allocate(size);
    if (!stagingRegion.isValid()) {
        return false;
    }
    
    memcpy(stagingRegion.mappedData, data, size);
    
    if (!bufferRegistry->getBufferFactory()) {
        return false;
    }
    
    // Create a temporary ResourceHandle for the staging buffer region
    ResourceHandle stagingHandle;
    stagingHandle.buffer = vulkan_raii::make_buffer(stagingRegion.buffer, bufferRegistry->getResourceContext()->getContext());
    stagingHandle.size = stagingPool->getTotalSize(); // Use total staging buffer size
    stagingHandle.mappedData = nullptr; // Staging buffer is already mapped, but we don't expose that here
    
    bufferRegistry->getBufferFactory()->copyBufferToBuffer(stagingHandle, dst, size, stagingRegion.offset, offset);
    
    // Detach to prevent cleanup of the staging buffer
    stagingHandle.buffer.detach();
    return true;
}

CommandExecutor::AsyncTransfer TransferOrchestrator::copyStagedToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!stagingPool || !executor || !data || size == 0 || !dst.isValid()) {
        return {};
    }
    
    auto stagingRegion = stagingPool->allocate(size);
    if (!stagingRegion.isValid()) {
        return {};
    }
    
    memcpy(stagingRegion.mappedData, data, size);
    
    return executor->copyBufferToBufferAsync(stagingRegion.buffer, dst.buffer.get(), size, stagingRegion.offset, offset);
}

void TransferOrchestrator::updateTransferStats(VkDeviceSize bytesTransferred, bool wasAsync, bool wasBatch) {
    transferStats.totalTransfers++;
    transferStats.totalBytesTransferred += bytesTransferred;
    
    if (wasAsync) {
        transferStats.asyncTransfers++;
    }
    
    if (wasBatch) {
        transferStats.batchTransfers++;
    }
}