#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "resource_handle.h"

class VulkanContext;
class MemoryAllocator;
class BufferFactory;

// Pure resource creation - single responsibility
class ResourceFactory {
public:
    ResourceFactory() = default;
    ~ResourceFactory() = default;
    
    bool initialize(const VulkanContext& context, MemoryAllocator* memoryAllocator);
    void cleanup();
    
    // Cleanup method for proper destruction order
    void cleanupBeforeContextDestruction();
    
    // Core resource creation operations
    ResourceHandle createBuffer(VkDeviceSize size, 
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties);
    
    ResourceHandle createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    ResourceHandle createImage(uint32_t width, uint32_t height,
                              VkFormat format,
                              VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    
    ResourceHandle createImageView(const ResourceHandle& imageHandle,
                                  VkFormat format,
                                  VkImageAspectFlags aspectFlags);
    
    void destroyResource(ResourceHandle& handle);
    
    // Access to underlying factory for advanced operations
    BufferFactory* getBufferFactory() const { return bufferFactory; }

private:
    BufferFactory* bufferFactory = nullptr;
    bool initialized = false;
};