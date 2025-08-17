#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../frame_graph.h"

// Forward declarations
class VulkanPipeline;
class GPUEntityManager;

class EntityComputeNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(EntityComputeNode)
    
public:
    EntityComputeNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        VulkanPipeline* pipeline,
        GPUEntityManager* gpuEntityManager
    );
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements
    bool needsComputeQueue() const override { return true; }
    bool needsGraphicsQueue() const override { return false; }
    
    // Frame state updates
    void updateFrameData(float time, float deltaTime, uint32_t frameCounter);

private:
    // Memory barrier insertion for compute->graphics synchronization
    void insertBufferBarriers(VkCommandBuffer commandBuffer, const VulkanContext* context);
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    
    // External dependencies (not owned)
    VulkanPipeline* pipeline;
    GPUEntityManager* gpuEntityManager;
    
    // Frame data for compute shader
    struct ComputePushConstants {
        float time;
        float deltaTime;
        uint32_t entityCount;
        uint32_t frame;
    } pushConstants{};
};