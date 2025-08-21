#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../core/vulkan_raii.h"
#include "memory_allocator.h"
#include "resource_handle.h"

class VulkanContext;
class StagingRingBuffer;
class CommandExecutor;

// Buffer and image creation factory
class BufferFactory {
public:
    BufferFactory();
    ~BufferFactory();
    
    bool initialize(const VulkanContext& context, MemoryAllocator* memoryAllocator);
    void cleanup();
    
    // Cleanup method for proper destruction order
    void cleanupBeforeContextDestruction();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Buffer creation helpers
    ResourceHandle createBuffer(VkDeviceSize size, 
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties);
    
    ResourceHandle createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    // Image creation helpers  
    ResourceHandle createImage(uint32_t width, uint32_t height,
                              VkFormat format,
                              VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    
    ResourceHandle createImageView(const ResourceHandle& imageHandle,
                                  VkFormat format,
                                  VkImageAspectFlags aspectFlags);
    
    // Resource destruction
    void destroyResource(ResourceHandle& handle);
    
    // Transfer operations (requires staging buffer and command executor)
    void setStagingBuffer(StagingRingBuffer* stagingBuffer) { this->stagingBuffer = stagingBuffer; }
    void setCommandExecutor(CommandExecutor* executor) { this->executor = executor; }
    
    void copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);

private:
    const VulkanContext* context = nullptr;
    MemoryAllocator* memoryAllocator = nullptr;
    StagingRingBuffer* stagingBuffer = nullptr;
    CommandExecutor* executor = nullptr;
};