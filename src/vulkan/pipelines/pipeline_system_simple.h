#pragma once

#include "../core/vulkan_context.h"
#include <memory>

// Simplified pipeline system that can replace VulkanPipeline gradually
class SimplePipelineManager {
public:
    SimplePipelineManager();
    ~SimplePipelineManager();

    bool initialize(const VulkanContext& context, VkFormat swapChainImageFormat);
    void cleanup();

    // Legacy compatibility methods
    VkRenderPass getRenderPass() const { return renderPass; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
    VkPipeline getComputePipeline() const { return computePipeline; }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout; }
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }

private:
    const VulkanContext* context = nullptr;
    
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;

    // Internal methods (simplified versions of the VulkanPipeline methods)
    bool createRenderPass(VkFormat swapChainImageFormat);
    bool createDescriptorSetLayout();
    bool createGraphicsPipeline();
    bool createComputeDescriptorSetLayout();
    bool createComputePipeline();
};