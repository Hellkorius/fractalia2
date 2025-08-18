#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <glm/glm.hpp>
#include "../core/vulkan_raii.h"

// Compute Pipeline State Object for caching
struct ComputePipelineState {
    // Shader information
    std::string shaderPath;
    std::vector<uint32_t> specializationConstants;  // For shader specialization
    
    // Descriptor set layouts
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    
    // Push constant ranges
    std::vector<VkPushConstantRange> pushConstantRanges;
    
    // Workgroup size hints (for optimization)
    uint32_t workgroupSizeX = 32;
    uint32_t workgroupSizeY = 1;
    uint32_t workgroupSizeZ = 1;
    
    // Performance hints
    bool isFrequentlyUsed = false;  // Hot path optimization
    bool allowAsyncCompilation = true;  // Background compilation
    
    // Comparison for caching
    bool operator==(const ComputePipelineState& other) const;
    size_t getHash() const;
};

// Hash specialization for compute PSO caching
struct ComputePipelineStateHash {
    std::size_t operator()(const ComputePipelineState& state) const {
        return state.getHash();
    }
};

// Cached compute pipeline with metadata
struct CachedComputePipeline {
    vulkan_raii::Pipeline pipeline;
    vulkan_raii::PipelineLayout layout;
    ComputePipelineState state;
    
    // Usage tracking
    uint64_t lastUsedFrame = 0;
    uint32_t useCount = 0;
    
    // Performance metrics
    std::chrono::nanoseconds compilationTime{0};
    bool isHotPath = false;
    
    // Dispatch optimization data
    struct DispatchInfo {
        glm::uvec3 optimalWorkgroupSize{32, 1, 1};
        uint32_t maxInvocationsPerWorkgroup = 1024;
        bool supportsSubgroupOperations = false;
    } dispatchInfo;
};

// Compute dispatch parameters with optimization
struct ComputeDispatch {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    
    // Dispatch dimensions
    uint32_t groupCountX = 1;
    uint32_t groupCountY = 1;
    uint32_t groupCountZ = 1;
    
    // Descriptor sets
    std::vector<VkDescriptorSet> descriptorSets;
    
    // Push constants
    const void* pushConstantData = nullptr;
    uint32_t pushConstantSize = 0;
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Memory barriers (for compute-compute dependencies)
    std::vector<VkMemoryBarrier> memoryBarriers;
    std::vector<VkBufferMemoryBarrier> bufferBarriers;
    std::vector<VkImageMemoryBarrier> imageBarriers;
    
    // Performance hints
    bool isLastDispatchInFrame = false;
    bool requiresMemoryBarrier = true;
    
    // Calculate optimal dispatch size based on data size
    void calculateOptimalDispatch(uint32_t dataSize, const glm::uvec3& workgroupSize);
};