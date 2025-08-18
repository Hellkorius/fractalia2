#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../frame_graph.h"
#include <memory>

// Forward declarations
class ComputePipelineManager;
class GPUEntityManager;
class GPUTimeoutDetector;

class EntityComputeNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(EntityComputeNode)
    
public:
    EntityComputeNode(
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
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector;
    
    // Adaptive dispatch parameters
    uint32_t adaptiveMaxWorkgroups = 512; // Conservative limit to prevent GPU timeouts
    bool forceChunkedDispatch = true;     // Always use chunking for stability
    
    // Frame data for compute shader
    struct ComputePushConstants {
        float time;
        float deltaTime;
        uint32_t entityCount;
        uint32_t frame;
        uint32_t entityOffset;  // For chunked dispatches
        uint32_t padding[3];    // Ensure 16-byte alignment
    } pushConstants{};
};