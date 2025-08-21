#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include "resource_handle.h"
#include "staging_buffer_manager.h"

class ResourceContext;

// GPU buffer with integrated staging support for compute operations
class GPUBufferRing {
public:
    GPUBufferRing() = default;
    ~GPUBufferRing();
    
    bool initialize(ResourceContext* resourceContext, VkDeviceSize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void cleanup();
    
    // Buffer access
    VkBuffer getBuffer() const { return storageHandle ? storageHandle->buffer.get() : VK_NULL_HANDLE; }
    void* getMappedData() const { return storageHandle ? storageHandle->mappedData : nullptr; }
    VkDeviceSize getSize() const { return bufferSize; }
    bool isValid() const { return storageHandle && storageHandle->isValid(); }
    
    // Staging operations for device-local buffers
    bool addData(const void* data, VkDeviceSize size, VkDeviceSize alignment = alignof(std::max_align_t));
    void flushToGPU(VkDeviceSize dstOffset = 0); // dstOffset for where to write in destination
    void resetStaging();
    bool hasPendingData() const { return needsUpload; }
    
    // Direct access to handle for special operations
    ResourceHandle* getHandle() { return storageHandle.get(); }
    const ResourceHandle* getHandle() const { return storageHandle.get(); }
    
private:
    std::unique_ptr<ResourceHandle> storageHandle;
    ResourceContext* resourceContext = nullptr;
    VkDeviceSize bufferSize = 0;
    
    // Staging state for device-local buffers
    VkDeviceSize stagingBytesWritten = 0;
    VkDeviceSize stagingStartOffset = 0;
    bool needsUpload = false;
    bool isDeviceLocal = false;
};

// GPU buffer management with multiple buffer types and automatic staging
class GPUBufferManager {
public:
    GPUBufferManager();
    ~GPUBufferManager();
    
    bool initialize(ResourceContext* resourceContext, StagingBufferManager* stagingManager);
    void cleanup();
    
    // Context access
    ResourceContext* getResourceContext() const { return resourceContext; }
    StagingBufferManager* getStagingManager() const { return stagingManager; }
    
    // Buffer creation
    std::unique_ptr<GPUBufferRing> createBuffer(VkDeviceSize size,
                                               VkBufferUsageFlags usage,
                                               VkMemoryPropertyFlags properties);
    
    // High-level buffer operations
    bool uploadData(GPUBufferRing& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void flushAll(); // Flush all pending staging operations
    void resetAllStaging(); // Reset all staging buffers
    
    // Buffer management
    struct BufferStats {
        uint32_t totalBuffers = 0;
        uint32_t deviceLocalBuffers = 0;
        uint32_t hostVisibleBuffers = 0;
        VkDeviceSize totalSize = 0;
        VkDeviceSize pendingStagingBytes = 0;
        uint32_t buffersWithPendingData = 0;
    };
    
    BufferStats getStats() const;
    bool hasPendingStagingOperations() const;
    
private:
    ResourceContext* resourceContext = nullptr;
    StagingBufferManager* stagingManager = nullptr;
    
    // Buffer tracking for statistics
    std::vector<GPUBufferRing*> managedBuffers;
    
    void registerBuffer(GPUBufferRing* buffer);
    void unregisterBuffer(GPUBufferRing* buffer);
    
    friend class GPUBufferRing; // Allow registration/unregistration
};