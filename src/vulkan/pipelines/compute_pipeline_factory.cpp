#include "compute_pipeline_factory.h"
#include "shader_manager.h"
#include "../core/vulkan_utils.h"
#include <iostream>
#include <cassert>

ComputePipelineFactory::ComputePipelineFactory(VulkanContext* ctx) : VulkanManagerBase(ctx) {}

bool ComputePipelineFactory::initialize(ShaderManager* shaderManager, vulkan_raii::PipelineCache& pipelineCache) {
    shaderManager_ = shaderManager;
    pipelineCache_ = &pipelineCache;
    return true;
}

std::unique_ptr<CachedComputePipeline> ComputePipelineFactory::createPipeline(const ComputePipelineState& state) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!validatePipelineState(state)) {
        std::cerr << "Invalid compute pipeline state" << std::endl;
        return nullptr;
    }
    
    auto cachedPipeline = std::make_unique<CachedComputePipeline>();
    cachedPipeline->state = state;
    
    VkPipelineLayout rawLayout = createPipelineLayout(state.descriptorSetLayouts, state.pushConstantRanges);
    if (rawLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute pipeline layout" << std::endl;
        return nullptr;
    }
    cachedPipeline->layout = vulkan_raii::make_pipeline_layout(rawLayout, context);
    
    VkShaderModule shaderModule = loadShader(state);
    if (shaderModule == VK_NULL_HANDLE) {
        std::cerr << "Failed to load compute shader: " << state.shaderPath << std::endl;
        return nullptr;
    }
    
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";
    
    VkSpecializationInfo specializationInfo{};
    std::vector<VkSpecializationMapEntry> mapEntries;
    if (!state.specializationConstants.empty()) {
        setupSpecializationInfo(state, specializationInfo, mapEntries);
        shaderStageInfo.pSpecializationInfo = &specializationInfo;
    }
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = cachedPipeline->layout.get();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    std::cout << "ComputePipelineFactory: Creating compute pipeline for shader: " << state.shaderPath << std::endl;
    
    VkPipeline rawPipeline;
    VkResult result = createComputePipelines(pipelineCache_->get(), 1, &pipelineInfo, &rawPipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "ComputePipelineFactory: Failed to create compute pipeline!" << std::endl;
        std::cerr << "  Shader path: " << state.shaderPath << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        return nullptr;
    }
    
    cachedPipeline->pipeline = vulkan_raii::make_pipeline(rawPipeline, context);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    cachedPipeline->compilationTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    
    logPipelineCreation(state, cachedPipeline->compilationTime);
    
    return cachedPipeline;
}

VkPipelineLayout ComputePipelineFactory::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                                             const std::vector<VkPushConstantRange>& pushConstants) {
    std::cout << "ComputePipelineFactory: Creating pipeline layout" << std::endl;
    std::cout << "  Descriptor set layouts count: " << setLayouts.size() << std::endl;
    std::cout << "  Push constant ranges count: " << pushConstants.size() << std::endl;
    
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
    layoutInfo.pPushConstantRanges = pushConstants.data();
    
    VkPipelineLayout layout;
    VkResult result = loader->vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
    
    if (result != VK_SUCCESS) {
        std::cerr << "ComputePipelineFactory: Failed to create pipeline layout!" << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        return VK_NULL_HANDLE;
    }
    
    std::cout << "ComputePipelineFactory: Pipeline layout created successfully: " << (void*)layout << std::endl;
    return layout;
}

bool ComputePipelineFactory::validatePipelineState(const ComputePipelineState& state) const {
    if (state.shaderPath.empty()) {
        std::cerr << "Compute pipeline state validation failed: empty shader path" << std::endl;
        return false;
    }
    
    if (state.workgroupSizeX == 0 || state.workgroupSizeY == 0 || state.workgroupSizeZ == 0) {
        std::cerr << "Compute pipeline state validation failed: invalid workgroup size" << std::endl;
        return false;
    }
    
    return true;
}

void ComputePipelineFactory::logPipelineCreation(const ComputePipelineState& state,
                                                std::chrono::nanoseconds compilationTime) const {
    std::cout << "Created compute pipeline: " << state.shaderPath 
              << " (compilation time: " << compilationTime.count() / 1000000.0f << "ms)" << std::endl;
}

VkShaderModule ComputePipelineFactory::loadShader(const ComputePipelineState& state) {
    return shaderManager_->loadSPIRVFromFile(state.shaderPath);
}

void ComputePipelineFactory::setupSpecializationInfo(const ComputePipelineState& state, 
                                                    VkSpecializationInfo& specializationInfo,
                                                    std::vector<VkSpecializationMapEntry>& mapEntries) {
    for (uint32_t i = 0; i < state.specializationConstants.size(); ++i) {
        VkSpecializationMapEntry entry{};
        entry.constantID = i;
        entry.offset = i * sizeof(uint32_t);
        entry.size = sizeof(uint32_t);
        mapEntries.push_back(entry);
    }
    
    specializationInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
    specializationInfo.pMapEntries = mapEntries.data();
    specializationInfo.dataSize = state.specializationConstants.size() * sizeof(uint32_t);
    specializationInfo.pData = state.specializationConstants.data();
}