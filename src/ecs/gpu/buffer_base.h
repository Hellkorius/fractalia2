#pragma once

#include "buffer_operations_interface.h"
#include <vulkan/vulkan.h>

// Forward declarations
class VulkanContext;
class ResourceContext;

/**
 * Base class providing common buffer operations to avoid code duplication
 * while allowing specialized buffer classes to maintain SRP
 */
class BufferBase : public IBufferOperations {
public:
    BufferBase();
    virtual ~BufferBase();
    
    // IBufferOperations interface
    VkBuffer getBuffer() const override { return buffer; }
    VkDeviceSize getSize() const override { return bufferSize; }
    uint32_t getMaxElements() const override { return maxElements; }
    bool isInitialized() const override { return buffer != VK_NULL_HANDLE; }
    
    // Common buffer operations
    bool copyData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0) override;
    
    // Lifecycle
    virtual bool initialize(const VulkanContext& context, ResourceContext* resourceContext, 
                           uint32_t maxElements, VkDeviceSize elementSize, VkBufferUsageFlags usage);
    virtual void cleanup();

protected:
    // Shared buffer resources
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = 0;
    VkDeviceSize elementSize = 0;
    uint32_t maxElements = 0;
    
    // Dependencies
    const VulkanContext* context = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Template method pattern - subclasses can override specific behavior
    virtual VkBufferUsageFlags getAdditionalUsageFlags() const { return 0; }
    virtual const char* getBufferTypeName() const = 0;
    
private:
    // Common implementation shared by all buffer types
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    void destroyBuffer();
};