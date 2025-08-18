#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <chrono>
#include "../core/vulkan_context.h"
#include "../core/vulkan_manager_base.h"
#include "../core/vulkan_raii.h"
#include "compute_pipeline_types.h"

class ShaderManager;
class DescriptorLayoutManager;

class ComputePipelineFactory : public VulkanManagerBase {
public:
    explicit ComputePipelineFactory(VulkanContext* ctx);
    ~ComputePipelineFactory() = default;

    bool initialize(ShaderManager* shaderManager, vulkan_raii::PipelineCache& pipelineCache);
    
    std::unique_ptr<CachedComputePipeline> createPipeline(const ComputePipelineState& state);
    VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                         const std::vector<VkPushConstantRange>& pushConstants);
    
    bool validatePipelineState(const ComputePipelineState& state) const;
    void logPipelineCreation(const ComputePipelineState& state, 
                           std::chrono::nanoseconds compilationTime) const;

private:
    ShaderManager* shaderManager_ = nullptr;
    vulkan_raii::PipelineCache* pipelineCache_ = nullptr;
    
    VkShaderModule loadShader(const ComputePipelineState& state);
    void setupSpecializationInfo(const ComputePipelineState& state, 
                               VkSpecializationInfo& specializationInfo,
                               std::vector<VkSpecializationMapEntry>& mapEntries);
};