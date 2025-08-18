#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../frame_graph.h"

// Forward declarations
class ComputePipelineManager;
class GPUEntityManager;

class EntityComputeNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(EntityComputeNode)
    
public:
    EntityComputeNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId currentPositionBuffer,
        FrameGraphTypes::ResourceId targetPositionBuffer,
        ComputePipelineManager* computeManager,
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
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId currentPositionBufferId;
    FrameGraphTypes::ResourceId targetPositionBufferId;
    
    // External dependencies (not owned)
    ComputePipelineManager* computeManager;
    GPUEntityManager* gpuEntityManager;
    
    // Frame data for compute shader
    struct ComputePushConstants {
        float time;
        float deltaTime;
        uint32_t entityCount;
        uint32_t frame;
    } pushConstants{};
};