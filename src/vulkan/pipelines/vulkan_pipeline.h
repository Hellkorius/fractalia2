#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include "../core/vulkan_context.h"
#include "../core/vulkan_raii.h"


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
    void cleanupBeforeContextDestruction();
    bool recreate(VkFormat swapChainImageFormat);

    VkRenderPass getRenderPass() const { return renderPass.get(); }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline.get(); }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout.get(); }
    
    // Unified compute pipeline support
    VkPipeline getComputePipeline() const { return computePipeline.get(); }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout; }
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout.get(); }

private:
    const VulkanContext* context = nullptr;
    
    vulkan_raii::RenderPass renderPass;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vulkan_raii::Pipeline graphicsPipeline;
    vulkan_raii::DescriptorSetLayout descriptorSetLayout;
    vulkan_raii::PipelineCache pipelineCache;
    
    // Unified compute pipeline resources
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE; // Unified pipeline layout
    vulkan_raii::DescriptorSetLayout computeDescriptorSetLayout; // Unified layout (3 bindings)
    vulkan_raii::Pipeline computePipeline;
    
    // Pipeline layout cache (instance-owned for proper cleanup)
    std::unordered_map<PipelineLayoutKey, vulkan_raii::PipelineLayout, PipelineLayoutKeyHash> pipelineLayoutCache;


    bool createRenderPass(VkFormat swapChainImageFormat);
    bool createDescriptorSetLayout();
    bool createGraphicsPipeline();
    VkPipelineLayout getOrCreatePipelineLayout(VkDescriptorSetLayout setLayout, 
                                              const VkPushConstantRange* pushConstantRange = nullptr);
    
    // Unified compute pipeline methods
    bool createComputeDescriptorSetLayout();
    bool createComputePipeline();
    
    
    std::array<VkVertexInputBindingDescription, 2> getVertexBindingDescriptions();
    std::array<VkVertexInputAttributeDescription, 10> getVertexAttributeDescriptions();
};