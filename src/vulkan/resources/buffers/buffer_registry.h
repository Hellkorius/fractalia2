#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include "../core/resource_handle.h"
#include "../core/resource_context_interface.h"
class BufferFactory;
class GPUBuffer;

class BufferRegistry {
public:
    BufferRegistry() = default;
    ~BufferRegistry();
    
    bool initialize(IResourceContext* resourceContext, BufferFactory* bufferFactory);
    void cleanup();
    
    std::unique_ptr<GPUBuffer> createBuffer(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags properties);
    
    void registerBuffer(GPUBuffer* buffer);
    void unregisterBuffer(GPUBuffer* buffer);
    
    struct RegistryStats {
        uint32_t totalBuffers = 0;
        uint32_t deviceLocalBuffers = 0;
        uint32_t hostVisibleBuffers = 0;
        VkDeviceSize totalBufferSize = 0;
        uint32_t buffersWithPendingData = 0;
    };
    
    RegistryStats getStats() const;
    bool hasPendingOperations() const;
    std::vector<GPUBuffer*> getBuffersWithPendingData() const;
    
    IResourceContext* getResourceContext() const { return resourceContext; }
    BufferFactory* getBufferFactory() const { return bufferFactory; }
    
private:
    IResourceContext* resourceContext = nullptr;
    BufferFactory* bufferFactory = nullptr;
    std::vector<GPUBuffer*> managedBuffers;
};