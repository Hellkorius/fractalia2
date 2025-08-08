#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include "vulkan_context.h"

// Forward declaration
class VulkanFunctionLoader;

class VulkanPipeline {
public:
    VulkanPipeline();
    ~VulkanPipeline();

    bool initialize(VulkanContext* context, VkFormat swapChainImageFormat, VulkanFunctionLoader* loader = nullptr);
    void cleanup();
    bool recreate(VkFormat swapChainImageFormat);

    VkRenderPass getRenderPass() const { return renderPass; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
    VulkanContext* context = nullptr;
    VulkanFunctionLoader* loader = nullptr;
    
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    // Note: Function pointers removed - now using centralized VulkanFunctionLoader

    bool createRenderPass(VkFormat swapChainImageFormat);
    bool createDescriptorSetLayout();
    bool createGraphicsPipeline();
    
    VkShaderModule createShaderModule(const std::vector<char>& code);
    // Note: readFile removed - now using VulkanUtils::readFile
    
    std::array<VkVertexInputBindingDescription, 2> getVertexBindingDescriptions();
    std::array<VkVertexInputAttributeDescription, 10> getVertexAttributeDescriptions();
};