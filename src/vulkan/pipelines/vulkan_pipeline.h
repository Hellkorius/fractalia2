#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include "../core/vulkan_context.h"
#include "../core/vulkan_raii.h"
#include "hash_utils.h"


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
        return VulkanHash::hash_shift_combine(
            std::hash<VkDescriptorSetLayout>{}(key.descriptorSetLayout),
            std::hash<uint32_t>{}(key.pushConstantRange.stageFlags),
            std::hash<uint32_t>{}(key.pushConstantRange.offset),
            std::hash<uint32_t>{}(key.pushConstantRange.size)
        );
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