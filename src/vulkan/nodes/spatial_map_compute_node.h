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

class SpatialMapComputeNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(SpatialMapComputeNode)
    
public:
    SpatialMapComputeNode(
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId spatialMapBuffer,
        FrameGraphTypes::ResourceId entityCellBuffer,
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
    
    // Node lifecycle
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;

private:
    // Spatial map push constants
    struct SpatialPushConstants {
        uint32_t entityCount;
        uint32_t gridResolution;        // 128
        float cellSize;                 // ~15.625
        float worldSize;                // 2000.0
        uint32_t maxEntitiesPerCell;    // 64
        uint32_t clearMapFirst;         // 1 to clear, 0 to skip
        uint32_t padding[2];            // Ensure 16-byte alignment
    } pushConstants{};
    
    // Helper method for chunked dispatch execution
    void executeChunkedDispatch(
        VkCommandBuffer commandBuffer, 
        const VulkanContext* context, 
        const class ComputeDispatch& dispatch,
        uint32_t totalWorkgroups,
        uint32_t maxWorkgroupsPerChunk,
        uint32_t entityCount);
    
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId spatialMapBufferId;
    FrameGraphTypes::ResourceId entityCellBufferId;
    
    // External dependencies (not owned) - validated during execution
    ComputePipelineManager* computeManager;
    GPUEntityManager* gpuEntityManager;
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector;
    
    // Adaptive dispatch parameters
    uint32_t adaptiveMaxWorkgroups = MAX_WORKGROUPS_PER_CHUNK;
    bool forceChunkedDispatch = true;     // Always use chunking for stability
    
    // Thread-safe debug counter
    mutable std::atomic<uint32_t> debugCounter{0};
    
    // Control whether to clear the spatial map each frame
    bool clearMapEachFrame = true;
};