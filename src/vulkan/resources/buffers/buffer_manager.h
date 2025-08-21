#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include "../core/resource_handle.h"
#include "../core/command_executor.h"
#include "staging_buffer_pool.h"
#include "buffer_registry.h"
#include "transfer_orchestrator.h"
#include "buffer_statistics_collector.h"

class ResourceContext;
class BufferFactory;
class GPUBuffer;

// Facade coordinating specialized buffer management components
class BufferManager {
public:
    BufferManager();
    ~BufferManager();
    
    bool initialize(const ResourceContext* resourceContext, 
                   BufferFactory* bufferFactory,
                   CommandExecutor* executor,
                   VkDeviceSize stagingSize = 16 * 1024 * 1024);
    void cleanup();
    
    // Context access
    const ResourceContext* getResourceContext() const;
    BufferFactory* getBufferFactory() const;
    CommandExecutor* getCommandExecutor() const;
    
    // Staging buffer operations (delegated to StagingBufferPool)
    StagingBufferPool& getPrimaryStagingBuffer();
    const StagingBufferPool& getPrimaryStagingBuffer() const;
    StagingBufferPool::StagingRegion allocateStaging(VkDeviceSize size, VkDeviceSize alignment = 1);
    StagingBufferPool::StagingRegionGuard allocateStagingGuarded(VkDeviceSize size, VkDeviceSize alignment = 1);
    void resetAllStaging();
    
    // GPU buffer operations (delegated to BufferRegistry)
    std::unique_ptr<GPUBuffer> createBuffer(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags properties);
    bool uploadData(GPUBuffer& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void flushAllBuffers();
    
    // Transfer operations (delegated to TransferOrchestrator)
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, 
                           VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, 
                                                     VkDeviceSize size, VkDeviceSize offset = 0);
    CommandExecutor::AsyncTransfer copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
                                                          VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Batch transfer operations
    using TransferBatch = TransferOrchestrator::TransferBatch;
    
    bool executeBatch(const TransferBatch& batch);
    CommandExecutor::AsyncTransfer executeBatchAsync(const TransferBatch& batch);
    
    // Advanced operations
    bool mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool tryOptimizeMemory();
    bool isTransferQueueAvailable() const;
    void flushPendingTransfers();
    
    // Statistics and monitoring (delegated to BufferStatisticsCollector)
    using BufferStats = BufferStatisticsCollector::BufferStats;
    
    BufferStats getStats() const;
    bool isUnderMemoryPressure() const;
    bool hasPendingStagingOperations() const;
    
    // Direct access to specialized components
    StagingBufferPool* getStagingPool() { return stagingPool.get(); }
    BufferRegistry* getBufferRegistry() { return bufferRegistry.get(); }
    TransferOrchestrator* getTransferOrchestrator() { return transferOrchestrator.get(); }
    BufferStatisticsCollector* getStatisticsCollector() { return statisticsCollector.get(); }
    
private:
    std::unique_ptr<StagingBufferPool> stagingPool;
    std::unique_ptr<BufferRegistry> bufferRegistry;
    std::unique_ptr<TransferOrchestrator> transferOrchestrator;
    std::unique_ptr<BufferStatisticsCollector> statisticsCollector;
    
    const ResourceContext* resourceContext = nullptr;
    BufferFactory* bufferFactory = nullptr;
    CommandExecutor* executor = nullptr;
};