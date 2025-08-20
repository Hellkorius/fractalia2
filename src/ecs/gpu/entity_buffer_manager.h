#pragma once

#include <vulkan/vulkan.h>
#include <memory>

// Forward declarations
class VulkanContext;
class ResourceContext;

/**
 * Manages GPU buffers for entity rendering system
 * Separates buffer management from entity logic for better modularity
 */
class EntityBufferManager {
public:
    EntityBufferManager();
    ~EntityBufferManager();

    bool initialize(const VulkanContext& context, ResourceContext* resourceContext, uint32_t maxEntities);
    void cleanup();
    
    // Buffer access
    VkBuffer getEntityBuffer() const { return entityBuffer; }
    VkBuffer getPositionBuffer() const { return positionBuffer; }
    VkBuffer getPositionBufferAlternate() const { return positionBufferAlternate; }
    VkBuffer getCurrentPositionBuffer() const { return currentPositionBuffer; }
    VkBuffer getTargetPositionBuffer() const { return targetPositionBuffer; }
    
    // Ping-pong buffer access for async compute
    VkBuffer getComputeWriteBuffer(uint32_t frameIndex) const;
    VkBuffer getGraphicsReadBuffer(uint32_t frameIndex) const;
    
    // Buffer properties
    VkDeviceSize getEntityBufferSize() const { return entityBufferSize; }
    VkDeviceSize getPositionBufferSize() const { return positionBufferSize; }
    uint32_t getMaxEntities() const { return maxEntities; }
    
    // Data upload
    void copyDataToBuffer(VkBuffer buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

private:
    // Dependencies
    const VulkanContext* context = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Buffer configuration
    uint32_t maxEntities = 0;
    VkDeviceSize entityBufferSize = 0;
    VkDeviceSize positionBufferSize = 0;
    
    // GPU buffers
    VkBuffer entityBuffer = VK_NULL_HANDLE;
    VkDeviceMemory entityBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer positionBuffer = VK_NULL_HANDLE;  
    VkDeviceMemory positionBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer positionBufferAlternate = VK_NULL_HANDLE;
    VkDeviceMemory positionBufferAlternateMemory = VK_NULL_HANDLE;
    
    VkBuffer currentPositionBuffer = VK_NULL_HANDLE;
    VkDeviceMemory currentPositionBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer targetPositionBuffer = VK_NULL_HANDLE;
    VkDeviceMemory targetPositionBufferMemory = VK_NULL_HANDLE;
    
    // Helper methods
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
};