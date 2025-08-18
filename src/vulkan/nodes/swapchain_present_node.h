#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"

// Forward declarations
class VulkanSwapchain;

class SwapchainPresentNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(SwapchainPresentNode)
    
public:
    SwapchainPresentNode(
        FrameGraphTypes::ResourceId colorTarget,
        VulkanSwapchain* swapchain
    );
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements - presentation happens on graphics queue
    bool needsComputeQueue() const override { return false; }
    bool needsGraphicsQueue() const override { return true; }
    
    // Update swapchain image index for current frame
    void setImageIndex(uint32_t imageIndex) { this->imageIndex = imageIndex; }

private:
    FrameGraphTypes::ResourceId colorTargetId;
    
    // External dependencies (not owned)
    VulkanSwapchain* swapchain;
    
    // Current frame state
    uint32_t imageIndex = 0;
};