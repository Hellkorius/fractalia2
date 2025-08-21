#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include "resource_handle.h"
#include "command_executor.h"
#include "staging_buffer_manager.h"

class ResourceContext;
class BufferFactory;

// Centralized transfer operations with staging coordination
class TransferManager {
public:
    TransferManager();
    ~TransferManager();
    
    bool initialize(const ResourceContext* resourceContext, 
                   BufferFactory* bufferFactory,
                   StagingBufferManager* stagingManager,
                   CommandExecutor* executor);
    void cleanup();
    
    // Context access
    const ResourceContext* getResourceContext() const { return resourceContext; }
    BufferFactory* getBufferFactory() const { return bufferFactory; }
    StagingBufferManager* getStagingManager() const { return stagingManager; }
    CommandExecutor* getCommandExecutor() const { return executor; }
    
    // Synchronous transfer operations
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, 
                           VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Asynchronous transfer operations
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, 
                                                     VkDeviceSize size, VkDeviceSize offset = 0);
    CommandExecutor::AsyncTransfer copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
                                                          VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Batch transfer operations
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
    
    bool executeBatch(const TransferBatch& batch);
    CommandExecutor::AsyncTransfer executeBatchAsync(const TransferBatch& batch);
    
    // Memory mapping utilities
    bool mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Transfer statistics and monitoring
    struct TransferStats {
        uint64_t totalTransfers = 0;
        uint64_t asyncTransfers = 0;
        uint64_t batchTransfers = 0;
        VkDeviceSize totalBytesTransferred = 0;
        VkDeviceSize stagingBytesUsed = 0;
        uint32_t activeStagingRegions = 0;
        float averageTransferSize = 0.0f;
    };
    
    TransferStats getStats() const;
    void resetStats();
    
    // Advanced features
    bool tryOptimizeTransfers(); // Attempt to optimize pending transfers
    bool isTransferQueueAvailable() const; // Check if dedicated transfer queue exists
    void flushPendingTransfers(); // Force completion of pending async transfers
    
private:
    const ResourceContext* resourceContext = nullptr;
    BufferFactory* bufferFactory = nullptr;
    StagingBufferManager* stagingManager = nullptr;
    CommandExecutor* executor = nullptr;
    
    // Statistics tracking
    mutable TransferStats stats;
    
    // Helper methods
    bool isBufferHostVisible(const ResourceHandle& buffer) const;
    bool requiresStaging(const ResourceHandle& buffer) const;
    StagingRingBuffer::StagingRegion allocateStaging(VkDeviceSize size, VkDeviceSize alignment = 1);
    
    // Internal transfer implementations
    bool copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    bool copyStagedToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    CommandExecutor::AsyncTransfer copyStagedToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    
    void updateStats(VkDeviceSize bytesTransferred, bool wasAsync = false, bool wasBatch = false);
};