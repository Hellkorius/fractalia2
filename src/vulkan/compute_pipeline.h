#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "vulkan_context.h"

class ComputePipeline {
public:
    ComputePipeline();
    ~ComputePipeline();

    bool initialize(VulkanContext* context);
    void cleanup();
    
    // Create compute pipeline for entity movement
    bool createMovementPipeline(VkDescriptorSetLayout descriptorSetLayout);
    
    VkPipeline getMovementPipeline() const { return movementPipeline; }
    VkPipelineLayout getMovementPipelineLayout() const { return movementPipelineLayout; }

private:
    VulkanContext* context = nullptr;
    
    VkPipeline movementPipeline = VK_NULL_HANDLE;
    VkPipelineLayout movementPipelineLayout = VK_NULL_HANDLE;

    // Function pointers for compute operations
    PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;

    void loadFunctions();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};