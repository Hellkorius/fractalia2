#include "buffer_statistics_collector.h"
#include "staging_buffer_pool.h"
#include "buffer_registry.h"
#include "transfer_orchestrator.h"

bool BufferStatisticsCollector::initialize(StagingBufferPool* stagingPool,
                                          BufferRegistry* bufferRegistry,
                                          TransferOrchestrator* transferOrchestrator) {
    if (!stagingPool || !bufferRegistry || !transferOrchestrator) {
        return false;
    }
    
    this->stagingPool = stagingPool;
    this->bufferRegistry = bufferRegistry;
    this->transferOrchestrator = transferOrchestrator;
    
    return true;
}

void BufferStatisticsCollector::cleanup() {
    stagingPool = nullptr;
    bufferRegistry = nullptr;
    transferOrchestrator = nullptr;
}

BufferStatisticsCollector::BufferStats BufferStatisticsCollector::getStats() const {
    BufferStats stats;
    
    if (!stagingPool || !bufferRegistry || !transferOrchestrator) {
        return stats;
    }
    
    // Staging buffer stats
    auto poolStats = stagingPool->getStats();
    stats.stagingTotalSize = poolStats.totalSize;
    stats.stagingFragmentedBytes = poolStats.fragmentedBytes;
    stats.stagingFragmentationRatio = poolStats.fragmentationRatio;
    stats.stagingFragmentationCritical = poolStats.fragmentationCritical;
    stats.stagingAllocations = poolStats.allocations;
    stats.stagingFailedAllocations = poolStats.failedAllocations;
    
    // GPU buffer stats
    auto registryStats = bufferRegistry->getStats();
    stats.totalBuffers = registryStats.totalBuffers;
    stats.deviceLocalBuffers = registryStats.deviceLocalBuffers;
    stats.hostVisibleBuffers = registryStats.hostVisibleBuffers;
    stats.totalBufferSize = registryStats.totalBufferSize;
    stats.buffersWithPendingData = registryStats.buffersWithPendingData;
    
    // Transfer stats
    auto transferStats = transferOrchestrator->getStats();
    stats.totalTransfers = transferStats.totalTransfers;
    stats.asyncTransfers = transferStats.asyncTransfers;
    stats.batchTransfers = transferStats.batchTransfers;
    stats.totalBytesTransferred = transferStats.totalBytesTransferred;
    stats.averageTransferSize = transferStats.averageTransferSize;
    
    return stats;
}

bool BufferStatisticsCollector::isUnderMemoryPressure() const {
    if (!stagingPool || !bufferRegistry) {
        return false;
    }
    
    return stagingPool->isUnderPressure();
}

bool BufferStatisticsCollector::hasPendingStagingOperations() const {
    if (!bufferRegistry) {
        return false;
    }
    
    return bufferRegistry->hasPendingOperations();
}

bool BufferStatisticsCollector::tryOptimizeMemory() {
    if (!stagingPool) {
        return false;
    }
    
    return stagingPool->tryDefragment();
}