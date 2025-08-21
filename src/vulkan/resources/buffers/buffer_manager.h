#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include "../core/resource_handle.h"
#include "../transfer/command_executor.h"

class VulkanContext;
class ResourceContext;
class BufferFactory;

// Staging ring buffer for efficient CPU->GPU transfers
class StagingRingBuffer {
public:
    bool initialize(const VulkanContext& context, VkDeviceSize size);
    void cleanup();
    
    struct StagingRegion {
        void* mappedData;
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
        
        bool isValid() const { return mappedData != nullptr && buffer != VK_NULL_HANDLE; }
    };
    
    // RAII wrapper for staging region allocation
    class StagingRegionGuard {
    public:
        StagingRegionGuard(StagingRingBuffer* buffer, VkDeviceSize size, VkDeviceSize alignment = 1)
            : stagingBuffer(buffer), region(buffer ? buffer->allocate(size, alignment) : StagingRegion{}) {}
        
        ~StagingRegionGuard() = default;
        
        StagingRegionGuard(const StagingRegionGuard&) = delete;
        StagingRegionGuard& operator=(const StagingRegionGuard&) = delete;
        
        StagingRegionGuard(StagingRegionGuard&& other) noexcept 
            : stagingBuffer(other.stagingBuffer), region(other.region) {
            other.stagingBuffer = nullptr;
            other.region = {};
        }
        
        StagingRegionGuard& operator=(StagingRegionGuard&& other) noexcept {
            if (this != &other) {
                stagingBuffer = other.stagingBuffer;
                region = other.region;
                other.stagingBuffer = nullptr;
                other.region = {};
            }
            return *this;
        }
        
        const StagingRegion& get() const { return region; }
        bool isValid() const { return region.isValid(); }
        
        const StagingRegion* operator->() const { return &region; }
        const StagingRegion& operator*() const { return region; }
        
    private:
        StagingRingBuffer* stagingBuffer;
        StagingRegion region;
    };
    
    StagingRegion allocate(VkDeviceSize size, VkDeviceSize alignment = 1);
    StagingRegionGuard allocateGuarded(VkDeviceSize size, VkDeviceSize alignment = 1);
    void reset();
    
    // Advanced memory management
    bool tryDefragment();
    VkDeviceSize getFragmentedBytes() const;
    bool isFragmentationCritical() const;
    
    VkBuffer getBuffer() const { return ringBuffer.buffer.get(); }
    
    // Public accessors for BufferManager
    VkDeviceSize getTotalSize() const { return totalSize; }
    
private:
    const VulkanContext* context = nullptr;
    ResourceHandle ringBuffer;
    VkDeviceSize currentOffset = 0;
    VkDeviceSize totalSize = 0;
    
    // Fragmentation tracking
    VkDeviceSize totalWastedBytes = 0;
    uint32_t wrapAroundCount = 0;
    VkDeviceSize largestFreeBlock = 0;
};

// Forward declare BufferManager for GPUBuffer
class BufferManager;

// GPU buffer with integrated staging support
class GPUBuffer {
public:
    GPUBuffer() = default;
    ~GPUBuffer();
    
    bool initialize(ResourceContext* resourceContext, BufferManager* bufferManager, VkDeviceSize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void cleanup();
    
    // Buffer access
    VkBuffer getBuffer() const { return storageHandle ? storageHandle->buffer.get() : VK_NULL_HANDLE; }
    void* getMappedData() const { return storageHandle ? storageHandle->mappedData : nullptr; }
    VkDeviceSize getSize() const { return bufferSize; }
    bool isValid() const { return storageHandle && storageHandle->isValid(); }
    
    // Data operations
    bool addData(const void* data, VkDeviceSize size, VkDeviceSize alignment = alignof(std::max_align_t));
    void flushToGPU(VkDeviceSize dstOffset = 0);
    void resetStaging();
    bool hasPendingData() const { return needsUpload; }
    
    ResourceHandle* getHandle() { return storageHandle.get(); }
    const ResourceHandle* getHandle() const { return storageHandle.get(); }
    
private:
    std::unique_ptr<ResourceHandle> storageHandle;
    ResourceContext* resourceContext = nullptr;
    BufferManager* bufferManager = nullptr;
    VkDeviceSize bufferSize = 0;
    
    // Staging state for device-local buffers
    VkDeviceSize stagingBytesWritten = 0;
    VkDeviceSize stagingStartOffset = 0;
    bool needsUpload = false;
    bool isDeviceLocal = false;
};

// Unified buffer management system combining staging, GPU buffers, and transfers
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
    const ResourceContext* getResourceContext() const { return resourceContext; }
    BufferFactory* getBufferFactory() const { return bufferFactory; }
    CommandExecutor* getCommandExecutor() const { return executor; }
    
    // Staging buffer operations
    StagingRingBuffer& getPrimaryStagingBuffer() { return primaryStagingBuffer; }
    const StagingRingBuffer& getPrimaryStagingBuffer() const { return primaryStagingBuffer; }
    StagingRingBuffer::StagingRegion allocateStaging(VkDeviceSize size, VkDeviceSize alignment = 1);
    StagingRingBuffer::StagingRegionGuard allocateStagingGuarded(VkDeviceSize size, VkDeviceSize alignment = 1);
    void resetAllStaging();
    
    // GPU buffer operations
    std::unique_ptr<GPUBuffer> createBuffer(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags properties);
    bool uploadData(GPUBuffer& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void flushAllBuffers();
    
    // Transfer operations
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, 
                           VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
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
    
    // Advanced operations
    bool mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool tryOptimizeMemory();
    bool isTransferQueueAvailable() const;
    void flushPendingTransfers();
    
    // Statistics and monitoring
    struct BufferStats {
        // Staging buffer stats
        VkDeviceSize stagingTotalSize = 0;
        VkDeviceSize stagingFragmentedBytes = 0;
        float stagingFragmentationRatio = 0.0f;
        bool stagingFragmentationCritical = false;
        uint32_t stagingAllocations = 0;
        
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
    
private:
    const ResourceContext* resourceContext = nullptr;
    BufferFactory* bufferFactory = nullptr;
    CommandExecutor* executor = nullptr;
    
    // Staging management
    StagingRingBuffer primaryStagingBuffer;
    uint32_t totalStagingAllocations = 0;
    uint32_t failedStagingAllocations = 0;
    
    // GPU buffer tracking
    std::vector<GPUBuffer*> managedBuffers;
    
    // Transfer statistics
    mutable struct {
        uint64_t totalTransfers = 0;
        uint64_t asyncTransfers = 0;
        uint64_t batchTransfers = 0;
        VkDeviceSize totalBytesTransferred = 0;
    } transferStats;
    
    // Helper methods
    bool isBufferHostVisible(const ResourceHandle& buffer) const;
    bool requiresStaging(const ResourceHandle& buffer) const;
    bool copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    bool copyStagedToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    CommandExecutor::AsyncTransfer copyStagedToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset);
    
    void registerBuffer(GPUBuffer* buffer);
    void unregisterBuffer(GPUBuffer* buffer);
    void updateTransferStats(VkDeviceSize bytesTransferred, bool wasAsync = false, bool wasBatch = false);
    
    friend class GPUBuffer; // Allow registration/unregistration
};