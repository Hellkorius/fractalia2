#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../frame_graph.h"

// Forward declarations
class VulkanPipeline;
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
        VulkanPipeline* pipeline,
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
    
    // Update frame data for vertex shader push constants
    void updateFrameData(float time, float deltaTime, uint32_t frameIndex) { 
        this->frameTime = time; 
        this->frameDeltaTime = deltaTime; 
        this->currentFrameIndex = frameIndex;
    }

private:
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId colorTargetId;
    
    // External dependencies (not owned)
    VulkanPipeline* pipeline;
    VulkanSwapchain* swapchain;
    ResourceContext* resourceContext;
    GPUEntityManager* gpuEntityManager;
    
    // Current frame state
    uint32_t imageIndex = 0;
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
};