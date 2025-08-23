#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"
#include <flecs.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <atomic>

// Forward declarations
class GraphicsPipelineManager;
class VulkanSwapchain;
class ResourceCoordinator;
class GPUEntityManager;

class ShadowPassNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(ShadowPassNode)
    
public:
    ShadowPassNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId shadowDepthTarget,
        GraphicsPipelineManager* graphicsManager,
        ResourceCoordinator* resourceCoordinator,
        GPUEntityManager* gpuEntityManager
    );
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements
    bool needsComputeQueue() const override { return false; }
    bool needsGraphicsQueue() const override { return true; }
    
    // Node lifecycle
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Set world reference for camera matrix access
    void setWorld(flecs::world* world) { this->world = world; }
    
    // Shadow configuration
    void setSunDirection(const glm::vec3& direction) { this->sunDirection = direction; shadowDataDirty = true; }
    void setShadowDistance(float distance) { this->shadowDistance = distance; shadowDataDirty = true; }
    void setCascadeCount(uint32_t count) { this->cascadeCount = glm::min(count, 4u); shadowDataDirty = true; }
    
    // Force shadow data update on next frame
    void markShadowDataDirty() { shadowDataDirty = true; }

private:
    // Shadow cascade calculation
    void calculateCascadedShadowMatrices();
    void updateShadowUniformBuffer();
    
    // Shadow uniform buffer structure
    struct ShadowUBO {
        glm::mat4 lightSpaceMatrices[4];  // Up to 4 cascades
        glm::vec4 cascadeSplits;          // Split distances for each cascade
        glm::vec4 sunDirection;           // w component unused
        float shadowDistance;
        uint32_t cascadeCount;
        float bias;
        float normalOffset;
    } shadowUBO{};
    
    // Resources
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId shadowDepthTargetId;
    
    // External dependencies (not owned)
    GraphicsPipelineManager* graphicsManager;
    ResourceCoordinator* resourceCoordinator;
    GPUEntityManager* gpuEntityManager;
    
    // Current frame state
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
    
    // ECS world reference for camera matrices
    flecs::world* world = nullptr;
    
    // Shadow configuration
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.3f, -0.8f, 0.5f));  // Interesting angle
    float shadowDistance = 1000.0f;
    uint32_t cascadeCount = 3;  // 3 cascades for good quality/performance balance
    
    bool shadowDataDirty = true;
    uint32_t lastUpdatedFrameIndex = UINT32_MAX;
    
    // Cascade split calculation
    std::vector<float> cascadeSplits;
    
    // Thread-safe debug counters
    mutable std::atomic<uint32_t> debugCounter{0};
    mutable std::atomic<uint32_t> renderCounter{0};
};