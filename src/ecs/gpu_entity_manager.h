#pragma once

#include "components/component.h"
#include "components/entity.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

// Forward declarations
class VulkanContext;
class VulkanSync;
class ResourceContext;

// GPU entity structure optimized for cache efficiency
struct GPUEntity {
    // Cache Line 1 (bytes 0-63) - HOT DATA: All frequently accessed in compute shaders
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, stateTimer, entityState
    glm::vec4 color;              // 16 bytes - RGBA color
    
    // Cache Line 2 (bytes 64-127) - COLD DATA: Rarely accessed
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
    
    GPUEntity() = default;
    
    // Create from ECS components
    static GPUEntity fromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern);
};

// Modular GPU Entity Manager for AAA Frame Graph Architecture
class GPUEntityManager {
public:
    GPUEntityManager();
    ~GPUEntityManager();

    bool initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext);
    void cleanup();
    
    // Entity management
    void addEntity(const GPUEntity& entity);
    void addEntitiesFromECS(const std::vector<flecs::entity>& entities);
    void uploadPendingEntities(); // Upload staged entities to GPU
    void clearAllEntities();
    
    // Direct buffer access for frame graph
    VkBuffer getEntityBuffer() const;
    VkBuffer getPositionBuffer() const;
    VkBuffer getPositionBufferAlternate() const; // For async compute ping-pong
    VkBuffer getCurrentPositionBuffer() const;
    VkBuffer getTargetPositionBuffer() const;
    
    // Async compute support - ping-pong between position buffers
    VkBuffer getComputeWriteBuffer(uint32_t frameIndex) const;  // Buffer compute writes to
    VkBuffer getGraphicsReadBuffer(uint32_t frameIndex) const;  // Buffer graphics reads from
    
    // Buffer properties
    VkDeviceSize getEntityBufferSize() const { return ENTITY_BUFFER_SIZE; }
    VkDeviceSize getPositionBufferSize() const { return POSITION_BUFFER_SIZE; }
    
    // Entity state
    uint32_t getEntityCount() const { return activeEntityCount; }
    uint32_t getMaxEntities() const { return MAX_ENTITIES; }
    bool hasPendingUploads() const { return !stagingEntities.empty(); }
    
    // Descriptor set support for frame graph
    bool createComputeDescriptorSets(VkDescriptorSetLayout layout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout layout);
    bool recreateComputeDescriptorSets(); // CRITICAL FIX: Recreate compute descriptor sets during swapchain recreation
    VkDescriptorSet getComputeDescriptorSet() const { return computeDescriptorSet; }
    VkDescriptorSet getGraphicsDescriptorSet() const { return graphicsDescriptorSet; }
    
    // Descriptor set layout creation for pipeline system integration
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }
    VkDescriptorSetLayout getGraphicsDescriptorSetLayout() const { return graphicsDescriptorSetLayout; }
    bool createDescriptorSetLayouts();

private:
    static constexpr uint32_t MAX_ENTITIES = 131072; // 128k entities max
    static constexpr size_t ENTITY_BUFFER_SIZE = MAX_ENTITIES * sizeof(GPUEntity);
    static constexpr size_t POSITION_BUFFER_SIZE = MAX_ENTITIES * sizeof(glm::vec4);
    
    // Dependencies
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // GPU buffers (simplified - no double buffering, frame graph handles that)
    VkBuffer entityBuffer = VK_NULL_HANDLE;
    VkDeviceMemory entityBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer positionBuffer = VK_NULL_HANDLE;  
    VkDeviceMemory positionBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer positionBufferAlternate = VK_NULL_HANDLE; // For async compute ping-pong
    VkDeviceMemory positionBufferAlternateMemory = VK_NULL_HANDLE;
    
    VkBuffer currentPositionBuffer = VK_NULL_HANDLE;
    VkDeviceMemory currentPositionBufferMemory = VK_NULL_HANDLE;
    
    VkBuffer targetPositionBuffer = VK_NULL_HANDLE;
    VkDeviceMemory targetPositionBufferMemory = VK_NULL_HANDLE;
    
    // Staging data
    std::vector<GPUEntity> stagingEntities;
    uint32_t activeEntityCount = 0;
    
    // Descriptor sets and layouts
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    
    // Helper methods
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyDataToBuffer(VkBuffer buffer, const void* data, VkDeviceSize size);
    bool createComputeDescriptorPool();
};