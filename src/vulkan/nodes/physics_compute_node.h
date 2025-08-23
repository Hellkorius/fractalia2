#pragma once

#include "base_compute_node.h"
#include <memory>

class PhysicsComputeNode : public BaseComputeNode {
    DECLARE_FRAME_GRAPH_NODE(PhysicsComputeNode)
    
public:
    PhysicsComputeNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId currentPositionBuffer,
        FrameGraphTypes::ResourceId targetPositionBuffer,
        ComputePipelineManager* computeManager,
        GPUEntityManager* gpuEntityManager,
        std::shared_ptr<GPUTimeoutDetector> timeoutDetector = nullptr
    );
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph, float time, float deltaTime) override;
    
protected:
    // BaseComputeNode virtual method implementations
    DispatchParams calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) override;
    ComputePipelineState createPipelineState(VkDescriptorSetLayout descriptorLayout) override;
    void setupPushConstants(float time, float deltaTime, uint32_t entityCount, uint32_t frameCounter) override;
    const char* getNodeName() const override { return "PhysicsComputeNode"; }
    const char* getDispatchBaseName() const override { return "Physics"; }
};