#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include "../core/resource_handle.h"

class ResourceContext;
class BufferManager;

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