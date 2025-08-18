#include "swapchain_present_node.h"
#include "../core/vulkan_swapchain.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include <iostream>

SwapchainPresentNode::SwapchainPresentNode(
    FrameGraphTypes::ResourceId colorTarget,
    VulkanSwapchain* swapchain
) : colorTargetId(colorTarget)
  , swapchain(swapchain) {
}

std::vector<ResourceDependency> SwapchainPresentNode::getInputs() const {
    return {
        {colorTargetId, ResourceAccess::Read, PipelineStage::ColorAttachment},
    };
}

std::vector<ResourceDependency> SwapchainPresentNode::getOutputs() const {
    // Present node doesn't produce frame graph resources, it presents to swapchain
    return {};
}

void SwapchainPresentNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    if (!swapchain) {
        std::cerr << "SwapchainPresentNode: Missing swapchain" << std::endl;
        return;
    }
    
    // Get Vulkan context from frame graph
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << "SwapchainPresentNode: Missing Vulkan context" << std::endl;
        return;
    }
    
    // Note: This node doesn't need to do anything in execute() for the command buffer
    // because the actual presentation happens on the queue level, not in command buffers.
    // The presentation logic will be handled by the frame graph execution system.
    // 
    // The important work of this node is declaring the dependency on the color target,
    // which ensures proper synchronization before presentation.
    
    // Ready for presentation
}