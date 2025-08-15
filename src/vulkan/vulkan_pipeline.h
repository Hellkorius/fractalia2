#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include "vulkan_context.h"


struct PipelineLayoutKey {
    VkDescriptorSetLayout descriptorSetLayout;
    VkPushConstantRange pushConstantRange;
    
    bool operator==(const PipelineLayoutKey& other) const {
        return descriptorSetLayout == other.descriptorSetLayout &&
               pushConstantRange.stageFlags == other.pushConstantRange.stageFlags &&
               pushConstantRange.offset == other.pushConstantRange.offset &&
               pushConstantRange.size == other.pushConstantRange.size;
    }
};

struct PipelineLayoutKeyHash {
    std::size_t operator()(const PipelineLayoutKey& key) const {
        std::size_t h1 = std::hash<VkDescriptorSetLayout>{}(key.descriptorSetLayout);
        std::size_t h2 = std::hash<uint32_t>{}(key.pushConstantRange.stageFlags);
        std::size_t h3 = std::hash<uint32_t>{}(key.pushConstantRange.offset);
        std::size_t h4 = std::hash<uint32_t>{}(key.pushConstantRange.size);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

class VulkanPipeline {
public:
    VulkanPipeline();
    ~VulkanPipeline();

    bool initialize(const VulkanContext& context, VkFormat swapChainImageFormat);
    void cleanup();
    bool recreate(VkFormat swapChainImageFormat);

    VkRenderPass getRenderPass() const { return renderPass; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
    
    // Modular compute pipeline support
    VkPipeline getRandomComputePipeline() const { return randomComputePipeline; }
    VkPipeline getPatternComputePipeline() const { return patternComputePipeline; }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout; }
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }

private:
    const VulkanContext* context = nullptr;
    
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    
    // Modular compute pipeline resources
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipeline randomComputePipeline = VK_NULL_HANDLE;
    VkPipeline patternComputePipeline = VK_NULL_HANDLE;
    
    // Pipeline layout cache (instance-owned for proper cleanup)
    std::unordered_map<PipelineLayoutKey, VkPipelineLayout, PipelineLayoutKeyHash> pipelineLayoutCache;


    bool createRenderPass(VkFormat swapChainImageFormat);
    bool createDescriptorSetLayout();
    bool createGraphicsPipeline();
    VkPipelineLayout getOrCreatePipelineLayout(VkDescriptorSetLayout setLayout, 
                                              const VkPushConstantRange* pushConstantRange = nullptr);
    
    // Modular compute pipeline methods
    bool createComputeDescriptorSetLayout();
    bool createModularComputePipelines();
    
    
    std::array<VkVertexInputBindingDescription, 2> getVertexBindingDescriptions();
    std::array<VkVertexInputAttributeDescription, 10> getVertexAttributeDescriptions();
};