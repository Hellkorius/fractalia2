#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"
#include "../core/vulkan_constants.h"
#include <memory>
#include <atomic>

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
    
    // Node lifecycle - standardized pattern
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Frame state updates - DEPRECATED: maintained for backward compatibility
    void updateFrameData(float time, float deltaTime, uint32_t frameCounter) override;

private:
    // Dispatch parameters struct
    struct DispatchParams {
        uint32_t totalWorkgroups;
        uint32_t maxWorkgroupsPerChunk;
        bool useChunking;
    };
    
    // Helper method for chunked dispatch execution
    void executeChunkedDispatch(
        VkCommandBuffer commandBuffer, 
        const VulkanContext* context, 
        const class ComputeDispatch& dispatch,
        uint32_t totalWorkgroups,
        uint32_t maxWorkgroupsPerChunk,
        uint32_t entityCount);
    
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId currentPositionBufferId;
    FrameGraphTypes::ResourceId targetPositionBufferId;
    
    // External dependencies (not owned) - validated during execution
    ComputePipelineManager* computeManager;
    GPUEntityManager* gpuEntityManager;
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector;
    
    // Adaptive dispatch parameters
    uint32_t adaptiveMaxWorkgroups = MAX_WORKGROUPS_PER_CHUNK;
    bool forceChunkedDispatch = true;     // Always use chunking for stability
    
    // Thread-safe debug counter
    mutable std::atomic<uint32_t> debugCounter{0};
    mutable std::atomic<uint32_t> frameCounter{0};
    
    // Frame timing data for new lifecycle
    float currentTime = 0.0f;
    float currentDeltaTime = 0.0f;
    
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