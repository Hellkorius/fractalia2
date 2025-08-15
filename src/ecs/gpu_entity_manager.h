#pragma once

#include "component.hpp"
#include "entity.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <queue>
#include <memory>

// GPU entity structure matching compute shader layout
struct GPUEntity {
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
    glm::vec4 color;              // 16 bytes - RGBA color  
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, stateTimer, entityState
    
    GPUEntity() = default;
    
    // Create from ECS components
    static GPUEntity fromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
        GPUEntity entity{};
        
        // Copy transform matrix
        entity.modelMatrix = transform.getMatrix();
        
        // Copy color
        entity.color = renderable.color;
        
        // Copy movement parameters
        entity.movementParams0 = glm::vec4(
            pattern.amplitude,
            pattern.frequency, 
            pattern.phase,
            pattern.timeOffset
        );
        
        entity.movementParams1 = glm::vec4(
            pattern.center.x,
            pattern.center.y, 
            pattern.center.z,
            static_cast<float>(pattern.type)
        );
        
        // Initialize runtime state
        entity.runtimeState = glm::vec4(
            pattern.totalTime,
            pattern.initialized ? 1.0f : 0.0f,
            0.0f, // stateTimer: timer for current state
            pattern.initialized ? 1.0f : 0.0f  // entityState: STATE_NORMAL or STATE_SPAWN_LERP
        );
        
        return entity;
    }
};

// Forward declarations
class VulkanContext;
class VulkanSync;
class ResourceContext;
struct ResourceHandle;

// Manages GPU entity storage and CPU->GPU handover
class GPUEntityManager {
public:
    GPUEntityManager();
    ~GPUEntityManager();
    
    bool initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext);
    void cleanup();
    
    // Entity management
    void addEntity(const GPUEntity& entity);
    void addEntitiesFromECS(const std::vector<Entity>& entities);
    void uploadPendingEntities();
    void clearAllEntities();
    void updateAllMovementTypes(int newMovementType, bool angelMode = false);
    
    // GPU buffer management  
    VkBuffer getCurrentEntityBuffer() const { return entityStorage; }
    uint32_t getComputeInputIndex() const { return 0; }
    uint32_t getComputeOutputIndex() const { return 0; }
    void advanceFrame() { /* No-op for single buffer */ }
    
    // Getters
    uint32_t getEntityCount() const { return activeEntityCount; }
    uint32_t getMaxEntities() const { return MAX_ENTITIES; }
    bool hasPendingUploads() const { return !pendingEntities.empty(); }
    
    // Descriptor set management
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }
    VkDescriptorSet getCurrentComputeDescriptorSet() const { return computeDescriptorSet; }
    
    bool createComputeDescriptorSets();

private:
    static constexpr uint32_t MAX_ENTITIES = 131072; // 128k entities max (16MB / 128 bytes)
    static constexpr size_t ENTITY_BUFFER_SIZE = MAX_ENTITIES * sizeof(GPUEntity);
    
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Single buffer storage for compute pipeline using ResourceContext
    std::unique_ptr<ResourceHandle> entityStorageHandle;
    VkBuffer entityStorage = VK_NULL_HANDLE;  // For compatibility
    void* entityBufferMapped = nullptr;
    
    // Entity state
    uint32_t activeEntityCount = 0;
    std::vector<GPUEntity> pendingEntities;
    
    
    // Compute descriptor sets for storage buffers
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    
    
    bool createEntityBuffers();
    bool createComputeDescriptorPool();
    bool createComputeDescriptorSetLayout();
};