#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "resource_handle.h"

class VulkanContext;
class MemoryAllocator;
class BufferFactory;
class CommandExecutor;

// Minimal interface for breaking circular dependencies
// This allows BufferManager to depend on an interface rather than concrete ResourceContext
class IResourceContext {
public:
    virtual ~IResourceContext() = default;
    
    // Context access
    virtual const VulkanContext* getContext() const = 0;
    
    // Core managers access
    virtual MemoryAllocator* getMemoryAllocator() const = 0;
    virtual BufferFactory* getBufferFactory() const = 0;
    virtual CommandExecutor* getCommandExecutor() const = 0;
    
    // Resource creation for BufferManager's internal needs
    virtual ResourceHandle createBuffer(VkDeviceSize size, 
                                       VkBufferUsageFlags usage,
                                       VkMemoryPropertyFlags properties) = 0;
    
    virtual ResourceHandle createMappedBuffer(VkDeviceSize size,
                                             VkBufferUsageFlags usage,
                                             VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) = 0;
    
    virtual bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, 
                                   VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) = 0;
    
    virtual void destroyResource(ResourceHandle& handle) = 0;
};