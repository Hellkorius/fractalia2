#pragma once

#include "specialized_buffers.h"
#include <vulkan/vulkan.h>

// Forward declarations
class VulkanContext;
class ResourceContext;

/**
 * SINGLE responsibility: coordinate ping-pong position buffer logic
 * Manages alternating read/write buffers for async compute/graphics pipeline
 */
class PositionBufferCoordinator {
public:
    PositionBufferCoordinator();
    ~PositionBufferCoordinator();
    
    bool initialize(const VulkanContext& context, ResourceContext* resourceContext, uint32_t maxEntities);
    void cleanup();
    
    // Ping-pong buffer access for async compute
    VkBuffer getComputeWriteBuffer(uint32_t frameIndex) const;
    VkBuffer getGraphicsReadBuffer(uint32_t frameIndex) const;
    
    // Direct buffer access
    VkBuffer getPrimaryBuffer() const { return primaryBuffer.getBuffer(); }
    VkBuffer getAlternateBuffer() const { return alternateBuffer.getBuffer(); }
    VkBuffer getCurrentBuffer() const { return currentBuffer.getBuffer(); }
    VkBuffer getTargetBuffer() const { return targetBuffer.getBuffer(); }
    
    // Buffer properties
    VkDeviceSize getBufferSize() const { return primaryBuffer.getSize(); }
    uint32_t getMaxEntities() const { return primaryBuffer.getMaxElements(); }
    
    // Data upload to all position buffers
    bool uploadToAllBuffers(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Individual buffer upload
    bool uploadToPrimary(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadToAlternate(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadToCurrent(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadToTarget(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // State
    bool isInitialized() const;

private:
    PositionBuffer primaryBuffer;        // Main position buffer (ping)
    PositionBuffer alternateBuffer;      // Alternate position buffer (pong)
    PositionBuffer currentBuffer;        // Current frame positions
    PositionBuffer targetBuffer;         // Target positions for interpolation
    
    uint32_t maxEntities = 0;
};