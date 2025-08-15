#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include "vulkan_context.h"

// Forward declaration
class VulkanFunctionLoader;

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
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    
    // Pipeline layout cache
    static std::unordered_map<PipelineLayoutKey, VkPipelineLayout, PipelineLayoutKeyHash> pipelineLayoutCache;

    // Note: Function pointers removed - now using centralized VulkanFunctionLoader

    bool createRenderPass(VkFormat swapChainImageFormat);
    bool createDescriptorSetLayout();
    bool createGraphicsPipeline();
    VkPipelineLayout getOrCreatePipelineLayout(VkDescriptorSetLayout setLayout, 
                                              const VkPushConstantRange* pushConstantRange = nullptr);
    
    // Note: createShaderModule and readFile removed - now using VulkanUtils
    
    std::array<VkVertexInputBindingDescription, 2> getVertexBindingDescriptions();
    std::array<VkVertexInputAttributeDescription, 10> getVertexAttributeDescriptions();
};