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
    
    // SoA buffer access
    VkBuffer getVelocityBuffer() const { return velocityBuffer; }
    VkBuffer getMovementParamsBuffer() const { return movementParamsBuffer; }
    VkBuffer getRuntimeStateBuffer() const { return runtimeStateBuffer; }
    VkBuffer getColorBuffer() const { return colorBuffer; }
    VkBuffer getModelMatrixBuffer() const { return modelMatrixBuffer; }
    
    // Position buffers
    VkBuffer getPositionBuffer() const { return positionBuffer; }
    VkBuffer getPositionBufferAlternate() const { return positionBufferAlternate; }
    VkBuffer getCurrentPositionBuffer() const { return currentPositionBuffer; }
    VkBuffer getTargetPositionBuffer() const { return targetPositionBuffer; }
    
    // Legacy support
    VkBuffer getEntityBuffer() const { return velocityBuffer; }
    
    // Ping-pong buffer access for async compute
    VkBuffer getComputeWriteBuffer(uint32_t frameIndex) const;
    VkBuffer getGraphicsReadBuffer(uint32_t frameIndex) const;
    
    // Buffer properties - SoA approach
    VkDeviceSize getVelocityBufferSize() const { return velocityBufferSize; }
    VkDeviceSize getMovementParamsBufferSize() const { return movementParamsBufferSize; }
    VkDeviceSize getRuntimeStateBufferSize() const { return runtimeStateBufferSize; }
    VkDeviceSize getColorBufferSize() const { return colorBufferSize; }
    VkDeviceSize getModelMatrixBufferSize() const { return modelMatrixBufferSize; }
    VkDeviceSize getPositionBufferSize() const { return positionBufferSize; }
    uint32_t getMaxEntities() const { return maxEntities; }
    
    // Legacy support
    VkDeviceSize getEntityBufferSize() const { return velocityBufferSize; }
    
    // Data upload
    void copyDataToBuffer(VkBuffer buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

private:
    // Dependencies
    const VulkanContext* context = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Buffer configuration
    uint32_t maxEntities = 0;
    VkDeviceSize velocityBufferSize = 0;
    VkDeviceSize movementParamsBufferSize = 0;
    VkDeviceSize runtimeStateBufferSize = 0;
    VkDeviceSize colorBufferSize = 0;
    VkDeviceSize modelMatrixBufferSize = 0;
    VkDeviceSize positionBufferSize = 0;
    
    // SoA GPU buffers
    VkBuffer velocityBuffer = VK_NULL_HANDLE;
    VkDeviceMemory velocityBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer movementParamsBuffer = VK_NULL_HANDLE;
    VkDeviceMemory movementParamsBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer runtimeStateBuffer = VK_NULL_HANDLE;
    VkDeviceMemory runtimeStateBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer colorBuffer = VK_NULL_HANDLE;
    VkDeviceMemory colorBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer modelMatrixBuffer = VK_NULL_HANDLE;
    VkDeviceMemory modelMatrixBufferMemory = VK_NULL_HANDLE;
    
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