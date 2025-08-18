#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"
#include <flecs.h>
#include <cstdint>
#include <glm/glm.hpp>

// Forward declarations
class GraphicsPipelineManager;
class VulkanSwapchain;
class ResourceContext;
class GPUEntityManager;

class EntityGraphicsNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(EntityGraphicsNode)
    
public:
    EntityGraphicsNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId colorTarget,
        GraphicsPipelineManager* graphicsManager,
        VulkanSwapchain* swapchain,
        ResourceContext* resourceContext,
        GPUEntityManager* gpuEntityManager
    );
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements
    bool needsComputeQueue() const override { return false; }
    bool needsGraphicsQueue() const override { return true; }
    
    // Update swapchain image index for current frame
    void setImageIndex(uint32_t imageIndex) { this->imageIndex = imageIndex; }
    
    // Update frame data for vertex shader push constants and uniform buffers
    void updateFrameData(float time, float deltaTime, uint32_t frameIndex) { 
        this->frameTime = time; 
        this->frameDeltaTime = deltaTime; 
        this->currentFrameIndex = frameIndex;
    }
    
    // Set world reference for camera matrix access
    void setWorld(flecs::world* world) { this->world = world; }
    
    // Force uniform buffer update on next frame (call when camera changes)
    void markUniformBufferDirty() { uniformBufferDirty = true; }

private:
    // Internal uniform buffer update
    void updateUniformBuffer();
    
    // Uniform buffer optimization - cache and dirty tracking
    struct CachedUBO {
        glm::mat4 view;
        glm::mat4 proj;
    } cachedUBO{};
    
    // Helper methods for camera matrix management
    CachedUBO getCameraMatrices();
    bool updateUniformBufferData(const CachedUBO& ubo);
    
    // Resources
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId colorTargetId;
    
    // External dependencies (not owned)
    GraphicsPipelineManager* graphicsManager;
    VulkanSwapchain* swapchain;
    ResourceContext* resourceContext;
    GPUEntityManager* gpuEntityManager;
    
    // Current frame state
    uint32_t imageIndex = 0;
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
    
    // ECS world reference for camera matrices
    flecs::world* world = nullptr;
    
    bool uniformBufferDirty = true;  // Force update on first frame
    uint32_t lastUpdatedFrameIndex = UINT32_MAX; // Track which frame index was last updated
};