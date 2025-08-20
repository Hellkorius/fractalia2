#pragma once

#include "../components/component.h"
#include "../components/entity.h"
#include "entity_buffer_manager.h"
#include "entity_descriptor_manager.h"
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
    glm::vec4 velocity;           // 16 bytes - velocity.xy, damping, reserved
    glm::vec4 movementParams;     // 16 bytes - amplitude, frequency, phase, timeOffset
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
    VkBuffer getEntityBuffer() const { return bufferManager.getEntityBuffer(); }
    VkBuffer getPositionBuffer() const { return bufferManager.getPositionBuffer(); }
    VkBuffer getPositionBufferAlternate() const { return bufferManager.getPositionBufferAlternate(); }
    VkBuffer getCurrentPositionBuffer() const { return bufferManager.getCurrentPositionBuffer(); }
    VkBuffer getTargetPositionBuffer() const { return bufferManager.getTargetPositionBuffer(); }
    
    // Async compute support - ping-pong between position buffers
    VkBuffer getComputeWriteBuffer(uint32_t frameIndex) const { return bufferManager.getComputeWriteBuffer(frameIndex); }
    VkBuffer getGraphicsReadBuffer(uint32_t frameIndex) const { return bufferManager.getGraphicsReadBuffer(frameIndex); }
    
    // Buffer properties
    VkDeviceSize getEntityBufferSize() const { return bufferManager.getEntityBufferSize(); }
    VkDeviceSize getPositionBufferSize() const { return bufferManager.getPositionBufferSize(); }
    
    // Entity state
    uint32_t getEntityCount() const { return activeEntityCount; }
    uint32_t getMaxEntities() const { return bufferManager.getMaxEntities(); }
    bool hasPendingUploads() const { return !stagingEntities.empty(); }
    
    // Descriptor management delegation
    EntityDescriptorManager& getDescriptorManager() { return descriptorManager; }
    const EntityDescriptorManager& getDescriptorManager() const { return descriptorManager; }

private:
    static constexpr uint32_t MAX_ENTITIES = 131072; // 128k entities max
    
    // Dependencies
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Core components
    EntityBufferManager bufferManager;
    EntityDescriptorManager descriptorManager;
    
    // Staging data
    std::vector<GPUEntity> stagingEntities;
    uint32_t activeEntityCount = 0;
};