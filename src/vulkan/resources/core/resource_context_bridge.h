#pragma once

#include "resource_context_interface.h"
#include "command_executor.h"

class ResourceCoordinator;
class MemoryAllocator;
class BufferFactory;

// Bridge implementation that provides ResourceContext interface using ResourceCoordinator
// This breaks the circular dependency between BufferManager and ResourceContext
class ResourceContextBridge : public IResourceContext {
public:
    ResourceContextBridge(ResourceCoordinator* coordinator, CommandExecutor* executor);
    ~ResourceContextBridge() override = default;
    
    // IResourceContext implementation
    const VulkanContext* getContext() const override;
    MemoryAllocator* getMemoryAllocator() const override;
    BufferFactory* getBufferFactory() const override;
    CommandExecutor* getCommandExecutor() const override;
    
    ResourceHandle createBuffer(VkDeviceSize size, 
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties) override;
    
    ResourceHandle createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) override;
    
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, 
                           VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) override;
    
    void destroyResource(ResourceHandle& handle) override;

private:
    ResourceCoordinator* coordinator;
    CommandExecutor* executor;
};