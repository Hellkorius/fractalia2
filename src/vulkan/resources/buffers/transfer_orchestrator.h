#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "../core/resource_handle.h"
#include "../core/command_executor.h"

class StagingBufferPool;
class BufferRegistry;

class TransferOrchestrator {
public:
    TransferOrchestrator() = default;
    ~TransferOrchestrator() = default;
    
    bool initialize(StagingBufferPool* stagingPool, 
                   BufferRegistry* bufferRegistry,
                   CommandExecutor* executor);
    void cleanup();
    
    struct TransferBatch {
        struct Transfer {
            const void* data;
            ResourceHandle* dstBuffer;
            VkDeviceSize size;
            VkDeviceSize offset;
        };
        
        std::vector<Transfer> transfers;
        
        void addTransfer(const void* data, ResourceHandle* dst, VkDeviceSize size, VkDeviceSize offset = 0) {
            transfers.push_back({data, dst, size, offset});
        }
        
        void clear() { transfers.clear(); }
        bool empty() const { return transfers.empty(); }
        size_t size() const { return transfers.size(); }
    };
    
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, 
                           VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, 
                                                     VkDeviceSize size, VkDeviceSize offset = 0);
    CommandExecutor::AsyncTransfer copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
                                                          VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    bool executeBatch(const TransferBatch& batch);
    CommandExecutor::AsyncTransfer executeBatchAsync(const TransferBatch& batch);
    
    bool mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void flushAllBuffers();
    
    bool isTransferQueueAvailable() const;
    void flushPendingTransfers();
    
    struct TransferStats {
        uint64_t totalTransfers = 0;
        uint64_t asyncTransfers = 0;
        uint64_t batchTransfers = 0;
        VkDeviceSize totalBytesTransferred = 0;
        float averageTransferSize = 0.0f;
    };
    
    TransferStats getStats() const;
    
private:
    StagingBufferPool* stagingPool = nullptr;
    BufferRegistry* bufferRegistry = nullptr;
    CommandExecutor* executor = nullptr;
    
    mutable struct {
        uint64_t totalTransfers = 0;
        uint64_t asyncTransfers = 0;
        uint64_t batchTransfers = 0;
        VkDeviceSize totalBytesTransferred = 0;
    } transferStats;
    
    bool isBufferHostVisible(const ResourceHandle& buffer) const;
    bool requiresStaging(const ResourceHandle& buffer) const;
    bool copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    bool copyStagedToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    CommandExecutor::AsyncTransfer copyStagedToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    
    void updateTransferStats(VkDeviceSize bytesTransferred, bool wasAsync = false, bool wasBatch = false);
};