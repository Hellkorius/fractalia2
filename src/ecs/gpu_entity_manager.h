#pragma once

#include "component.h"
#include "entity.h"
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
        
        // Initialize runtime state with random staggering for RandomStep movement
        float initialTimer = 0.0f;
        float initialState = 0.0f; // Start in COMPUTING state
        
        if (pattern.type == MovementType::RandomStep) {
            // Stagger entities to avoid all computing at once
            // Use phase as a seed for random initial timer
            uint32_t seed = static_cast<uint32_t>(pattern.phase * 10000.0f);
            seed = (seed ^ 61u) ^ (seed >> 16u);
            seed *= 9u;
            float randomFactor = static_cast<float>(seed % 1000) / 1000.0f;
            
            // Random initial timer between 0-600 frames
            initialTimer = randomFactor * 600.0f;
            initialState = 1.0f; // Start in INTERPOLATING state with random timer
        }
        
        entity.runtimeState = glm::vec4(
            pattern.totalTime,
            pattern.initialized ? 1.0f : 0.0f,
            initialTimer, // stateTimer: interpolation frame counter (0-600)
            initialState  // entityState: 0=COMPUTING, 1=INTERPOLATING
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
    
    bool initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext, VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE);
    void cleanup();
    
    // Entity management
    void addEntity(const GPUEntity& entity);
    void addEntitiesFromECS(const std::vector<Entity>& entities);
    void uploadPendingEntities();
    void clearAllEntities();
    void updateAllMovementTypes(int newMovementType, bool angelMode = false);
    
    // GPU buffer management  
    VkBuffer getCurrentEntityBuffer() const { return entityStorage; }
    VkBuffer getCurrentPositionBuffer() const { return positionStorage; }
    VkBuffer getCurrentPositionStorageBuffer() const { return currentPositionStorage; }
    VkBuffer getTargetPositionStorageBuffer() const { return targetPositionStorage; }
    uint32_t getComputeInputIndex() const { return 0; }
    uint32_t getComputeOutputIndex() const { return 0; }
    void advanceFrame() { /* No-op for single buffer */ }
    
    // Getters
    uint32_t getEntityCount() const { return activeEntityCount; }
    uint32_t getMaxEntities() const { return MAX_ENTITIES; }
    bool hasPendingUploads() const { return !pendingEntities.empty(); }
    
    // Safe access to mapped data through ResourceHandle
    void* getMappedData() const;
    
    // Descriptor set management
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }
    VkDescriptorSet getCurrentComputeDescriptorSet() const { return computeDescriptorSet; }
    
    bool createComputeDescriptorSets();
    void setComputeDescriptorSetLayout(VkDescriptorSetLayout layout) { computeDescriptorSetLayout = layout; }
    bool recreateComputeDescriptorResources();

private:
    static constexpr uint32_t MAX_ENTITIES = 131072; // 128k entities max (16MB / 128 bytes)
    static constexpr size_t ENTITY_BUFFER_SIZE = MAX_ENTITIES * sizeof(GPUEntity);
    static constexpr size_t POSITION_BUFFER_SIZE = MAX_ENTITIES * sizeof(glm::vec4); // 16 bytes per position
    
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Single buffer storage for compute pipeline using ResourceContext
    std::unique_ptr<ResourceHandle> entityStorageHandle;
    VkBuffer entityStorage = VK_NULL_HANDLE;  // For compatibility
    
    // Position buffer for compute shader output
    std::unique_ptr<ResourceHandle> positionStorageHandle;
    VkBuffer positionStorage = VK_NULL_HANDLE;
    
    // Buffers for random walk interpolation
    std::unique_ptr<ResourceHandle> currentPositionStorageHandle;
    VkBuffer currentPositionStorage = VK_NULL_HANDLE;  // Current position for interpolation
    
    std::unique_ptr<ResourceHandle> targetPositionStorageHandle;
    VkBuffer targetPositionStorage = VK_NULL_HANDLE;   // Target position for interpolation
    
    // Entity state
    uint32_t activeEntityCount = 0;
    std::vector<GPUEntity> pendingEntities;
    
    
    // Single compute descriptor set for unified pipeline
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE; // Unified layout (4 bindings)
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    bool ownsDescriptorSetLayout = false; // Track if we need to destroy the layout
    
    
    bool createEntityBuffers();
    bool createComputeDescriptorPool();
    bool createComputeDescriptorSetLayout();
};