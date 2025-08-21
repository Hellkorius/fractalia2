#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

class StagingBufferPool;
class BufferRegistry;
class TransferOrchestrator;

class BufferStatisticsCollector {
public:
    BufferStatisticsCollector() = default;
    ~BufferStatisticsCollector() = default;
    
    bool initialize(StagingBufferPool* stagingPool,
                   BufferRegistry* bufferRegistry,
                   TransferOrchestrator* transferOrchestrator);
    void cleanup();
    
    struct BufferStats {
        // Staging buffer stats
        VkDeviceSize stagingTotalSize = 0;
        VkDeviceSize stagingFragmentedBytes = 0;
        float stagingFragmentationRatio = 0.0f;
        bool stagingFragmentationCritical = false;
        uint32_t stagingAllocations = 0;
        uint32_t stagingFailedAllocations = 0;
        
        // GPU buffer stats
        uint32_t totalBuffers = 0;
        uint32_t deviceLocalBuffers = 0;
        uint32_t hostVisibleBuffers = 0;
        VkDeviceSize totalBufferSize = 0;
        uint32_t buffersWithPendingData = 0;
        
        // Transfer stats
        uint64_t totalTransfers = 0;
        uint64_t asyncTransfers = 0;
        uint64_t batchTransfers = 0;
        VkDeviceSize totalBytesTransferred = 0;
        float averageTransferSize = 0.0f;
    };
    
    BufferStats getStats() const;
    bool isUnderMemoryPressure() const;
    bool hasPendingStagingOperations() const;
    bool tryOptimizeMemory();
    
private:
    StagingBufferPool* stagingPool = nullptr;
    BufferRegistry* bufferRegistry = nullptr;
    TransferOrchestrator* transferOrchestrator = nullptr;
};