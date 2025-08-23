#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../frame_graph_types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

// Forward declarations
class VulkanContext;
class FrameGraphNode;

// Forward declare resource types - they will be provided via function injection
namespace FrameGraphResources {
    struct FrameGraphBuffer;
    struct FrameGraphImage;
}

namespace FrameGraphExecution {

// Per-node barrier tracking for optimal async execution
struct NodeBarrierInfo {
    std::vector<VkBufferMemoryBarrier> bufferBarriers;
    std::vector<VkImageMemoryBarrier> imageBarriers;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    FrameGraphTypes::NodeId targetNodeId = 0;
    
    void clear() {
        bufferBarriers.clear();
        imageBarriers.clear();
        targetNodeId = 0;
    }
};

// Resource write tracking for O(n) barrier analysis
struct ResourceWriteInfo {
    FrameGraphTypes::NodeId writerNode = 0;
    PipelineStage stage = PipelineStage::ComputeShader;
    ResourceAccess access = ResourceAccess::Read;
};

class BarrierManager {
public:
    BarrierManager() = default;
    ~BarrierManager() = default;

    // Initialize with context for barrier operations
    void initialize(const VulkanContext* context);

    // Main barrier analysis interface
    void analyzeBarrierRequirements(const std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                    const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);

    void createOptimalBarrierBatches(const std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                     const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);

    // Execution time barrier insertion
    void insertBarriersForNode(FrameGraphTypes::NodeId nodeId, VkCommandBuffer graphicsCmd, 
                               bool& computeExecuted, bool nodeNeedsGraphics);

    // Resource access helpers
    void setResourceAccessors(std::function<const FrameGraphResources::FrameGraphBuffer*(FrameGraphTypes::ResourceId)> getBuffer,
                              std::function<const FrameGraphResources::FrameGraphImage*(FrameGraphTypes::ResourceId)> getImage);

    // Reset for next frame
    void reset();

private:
    // Core barrier analysis
    void addResourceBarrier(FrameGraphTypes::ResourceId resourceId, FrameGraphTypes::NodeId targetNode,
                           PipelineStage srcStage, PipelineStage dstStage,
                           ResourceAccess srcAccess, ResourceAccess dstAccess);

    FrameGraphTypes::NodeId findNextGraphicsNode(FrameGraphTypes::NodeId fromNode,
                                                  const std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                                  const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) const;

    void insertBarrierBatch(const NodeBarrierInfo& batch, VkCommandBuffer commandBuffer);

    // Access conversion helpers
    VkAccessFlags convertAccess(ResourceAccess access, PipelineStage stage) const;
    VkPipelineStageFlags convertPipelineStage(PipelineStage stage) const;

    // State
    const VulkanContext* context_ = nullptr;
    
    // Resource write tracking for O(n) barrier analysis
    std::unordered_map<FrameGraphTypes::ResourceId, ResourceWriteInfo> resourceWriteTracking_;
    
    // Barrier batches inserted at optimal points for async execution
    std::vector<NodeBarrierInfo> barrierBatches_;

    // Resource accessors (injected dependencies)
    std::function<const FrameGraphResources::FrameGraphBuffer*(FrameGraphTypes::ResourceId)> getBufferResource_;
    std::function<const FrameGraphResources::FrameGraphImage*(FrameGraphTypes::ResourceId)> getImageResource_;
};

} // namespace FrameGraphExecution