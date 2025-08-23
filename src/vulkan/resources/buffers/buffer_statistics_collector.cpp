#include "buffer_statistics_collector.h"
#include "staging_buffer_pool.h"
#include "buffer_registry.h"
#include "transfer_orchestrator.h"
#include "../core/validation_utils.h"

bool BufferStatisticsCollector::initialize(StagingBufferPool* stagingPool,
                                          BufferRegistry* bufferRegistry,
                                          TransferOrchestrator* transferOrchestrator) {
    if (!ValidationUtils::validateDependencies("BufferStatisticsCollector::initialize",
                                               stagingPool, bufferRegistry, transferOrchestrator)) {
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
    stats.markValid();
    
    if (!stagingPool || !bufferRegistry || !transferOrchestrator) {
        stats.markInvalid();
        return stats;
    }
    
    // Collect staging buffer stats
    auto stagingStats = stagingPool->getStats();
    stats.stagingTotalSize = stagingStats.totalSize;
    stats.stagingFragmentedBytes = stagingStats.fragmentedBytes;
    stats.stagingFragmentationRatio = stagingStats.fragmentationRatio;
    stats.stagingFragmentationCritical = stagingStats.fragmentationCritical;
    stats.stagingAllocations = stagingStats.allocations;
    stats.stagingFailedAllocations = stagingStats.failedAllocations;
    
    // Collect buffer registry stats
    auto registryStats = bufferRegistry->getStats();
    stats.totalBuffers = registryStats.totalBuffers;
    stats.deviceLocalBuffers = registryStats.deviceLocalBuffers;
    stats.hostVisibleBuffers = registryStats.hostVisibleBuffers;
    stats.totalBufferSize = registryStats.totalBufferSize;
    stats.buffersWithPendingData = registryStats.buffersWithPendingData;
    
    // Collect transfer stats
    auto transferStats = transferOrchestrator->getStats();
    stats.totalTransfers = transferStats.totalTransfers;
    stats.asyncTransfers = transferStats.asyncTransfers;
    stats.batchTransfers = transferStats.batchTransfers;
    stats.totalBytesTransferred = transferStats.totalBytesTransferred;
    stats.averageTransferSize = transferStats.averageTransferSize;
    
    return stats;
}

bool BufferStatisticsCollector::isUnderMemoryPressure() const {
    if (!stagingPool) return false;
    
    auto stats = stagingPool->getStats();
    return stats.fragmentationCritical || (stats.fragmentationRatio > 0.8f);
}

bool BufferStatisticsCollector::hasPendingStagingOperations() const {
    if (!stagingPool) return false;
    
    auto stats = stagingPool->getStats();
    return stats.allocations > 0;
}

bool BufferStatisticsCollector::tryOptimizeMemory() {
    bool success = true;
    
    if (stagingPool) {
        success &= stagingPool->tryDefragment();
    }
    
    return success;
}