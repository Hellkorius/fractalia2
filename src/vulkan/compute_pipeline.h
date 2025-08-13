#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "vulkan_context.h"

class VulkanFunctionLoader;

class ComputePipeline {
public:
    ComputePipeline();
    ~ComputePipeline();

    bool initialize(VulkanContext* context, VulkanFunctionLoader* loader = nullptr);
    void cleanup();
    
    // Create compute pipeline for entity movement
    bool createMovementPipeline(VkDescriptorSetLayout descriptorSetLayout);
    
    // Create compute pipeline for keyframe generation
    bool createKeyframePipeline(VkDescriptorSetLayout descriptorSetLayout);
    
    VkPipeline getMovementPipeline() const { return movementPipeline; }
    VkPipelineLayout getMovementPipelineLayout() const { return movementPipelineLayout; }
    VkPipeline getKeyframePipeline() const { return keyframePipeline; }
    VkPipelineLayout getKeyframePipelineLayout() const { return keyframePipelineLayout; }

private:
    VulkanContext* context = nullptr;
    VulkanFunctionLoader* loader = nullptr;
    
    VkPipeline movementPipeline = VK_NULL_HANDLE;
    VkPipelineLayout movementPipelineLayout = VK_NULL_HANDLE;
    VkPipeline keyframePipeline = VK_NULL_HANDLE;
    VkPipelineLayout keyframePipelineLayout = VK_NULL_HANDLE;

    // Note: Function pointers removed - now using centralized VulkanFunctionLoader
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};