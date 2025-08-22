#pragma once

#include <vulkan/vulkan.h>

/**
 * Interface for buffer operations to avoid code duplication
 * while maintaining Single Responsibility Principle
 */
class IBufferOperations {
public:
    virtual ~IBufferOperations() = default;
    
    // Core buffer access
    virtual VkBuffer getBuffer() const = 0;
    virtual VkDeviceSize getSize() const = 0;
    virtual uint32_t getMaxElements() const = 0;
    
    // Data upload operations
    virtual bool copyData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0) = 0;
    
    // Data readback operations (expensive - use sparingly)
    virtual bool readData(void* data, VkDeviceSize size, VkDeviceSize offset = 0) const = 0;
    
    // Buffer state
    virtual bool isInitialized() const = 0;
};