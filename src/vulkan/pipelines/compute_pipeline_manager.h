#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <future>
#include <glm/glm.hpp>
#include "../core/vulkan_context.h"

// Forward declarations
class ShaderManager;
class DescriptorLayoutManager;

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
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
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

// AAA Compute Pipeline Manager with advanced optimization
class ComputePipelineManager {
public:
    ComputePipelineManager();
    ~ComputePipelineManager();

    // Initialization
    bool initialize(const VulkanContext& context, 
                   ShaderManager* shaderManager,
                   DescriptorLayoutManager* layoutManager);
    void cleanup();

    // Pipeline creation and caching
    VkPipeline getPipeline(const ComputePipelineState& state);
    VkPipelineLayout getPipelineLayout(const ComputePipelineState& state);
    
    // Batch pipeline creation for reduced driver overhead
    std::vector<VkPipeline> createPipelinesBatch(const std::vector<ComputePipelineState>& states);
    
    // High-level dispatch interface
    void dispatch(VkCommandBuffer commandBuffer, const ComputeDispatch& dispatch);
    void dispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset);
    
    // Optimized dispatch patterns
    void dispatchBuffer(VkCommandBuffer commandBuffer, const ComputePipelineState& state,
                       uint32_t elementCount, const std::vector<VkDescriptorSet>& descriptorSets,
                       const void* pushConstants = nullptr, uint32_t pushConstantSize = 0);
    
    void dispatchImage(VkCommandBuffer commandBuffer, const ComputePipelineState& state,
                      uint32_t width, uint32_t height, const std::vector<VkDescriptorSet>& descriptorSets,
                      const void* pushConstants = nullptr, uint32_t pushConstantSize = 0);
    
    // Default PSO presets for common compute patterns
    ComputePipelineState createBufferProcessingState(const std::string& shaderPath, 
                                                    VkDescriptorSetLayout descriptorLayout);
    ComputePipelineState createImageProcessingState(const std::string& shaderPath,
                                                   VkDescriptorSetLayout descriptorLayout);
    ComputePipelineState createParticleSystemState(const std::string& shaderPath,
                                                  VkDescriptorSetLayout descriptorLayout);
    
    // Cache management
    void warmupCache(const std::vector<ComputePipelineState>& commonStates);
    void optimizeCache(uint64_t currentFrame);
    void clearCache();
    
    // Async compilation for hot reloading
    bool compileAsync(const ComputePipelineState& state);
    bool isAsyncCompilationComplete(const ComputePipelineState& state);
    
    // Performance profiling
    struct ComputeProfileData {
        std::chrono::nanoseconds lastDispatchTime{0};
        uint64_t totalDispatches = 0;
        uint64_t totalWorkgroups = 0;
        float averageWorkgroupUtilization = 0.0f;
    };
    
    void beginProfiling(const ComputePipelineState& state);
    void endProfiling(const ComputePipelineState& state);
    ComputeProfileData getProfilingData(const ComputePipelineState& state) const;
    
    // Statistics and debugging
    struct ComputeStats {
        uint32_t totalPipelines = 0;
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        uint32_t dispatchesThisFrame = 0;
        uint64_t totalDispatches = 0;
        std::chrono::nanoseconds totalCompilationTime{0};
        float hitRatio = 0.0f;
    };
    
    ComputeStats getStats() const { return stats; }
    void resetFrameStats();
    void debugPrintCache() const;
    
    // Workgroup optimization
    glm::uvec3 calculateOptimalWorkgroupSize(uint32_t dataSize, 
                                            const glm::uvec3& maxWorkgroupSize = {1024, 1024, 64}) const;
    uint32_t calculateOptimalWorkgroupCount(uint32_t dataSize, uint32_t workgroupSize) const;
    
    // Memory barrier optimization
    void insertOptimalBarriers(VkCommandBuffer commandBuffer, 
                              const std::vector<VkBufferMemoryBarrier>& bufferBarriers,
                              const std::vector<VkImageMemoryBarrier>& imageBarriers,
                              VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    
    // Access to layout manager for descriptor layout creation
    DescriptorLayoutManager* getLayoutManager() { return layoutManager; }
    const DescriptorLayoutManager* getLayoutManager() const { return layoutManager; }

private:
    // Core Vulkan objects
    const VulkanContext* context = nullptr;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    
    // Dependencies
    ShaderManager* shaderManager = nullptr;
    DescriptorLayoutManager* layoutManager = nullptr;
    
    // Pipeline cache
    std::unordered_map<ComputePipelineState, std::unique_ptr<CachedComputePipeline>, ComputePipelineStateHash> pipelineCache_;
    
    // Async compilation tracking
    std::unordered_map<ComputePipelineState, std::future<std::unique_ptr<CachedComputePipeline>>, ComputePipelineStateHash> asyncCompilations;
    
    // Performance tracking
    std::unordered_map<ComputePipelineState, ComputeProfileData, ComputePipelineStateHash> profileData;
    
    // Statistics
    mutable ComputeStats stats;
    
    // Device capabilities (cached at initialization)
    VkPhysicalDeviceProperties deviceProperties{};
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    // Configuration
    uint32_t maxCacheSize = 512;
    uint64_t cacheCleanupInterval = 1000;  // frames
    bool enableProfiling = false;
    
    // Internal pipeline creation
    std::unique_ptr<CachedComputePipeline> createPipelineInternal(const ComputePipelineState& state);
    VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                         const std::vector<VkPushConstantRange>& pushConstants);
    
    // Cache management helpers
    void evictLeastRecentlyUsed();
    bool shouldEvictPipeline(const CachedComputePipeline& pipeline, uint64_t currentFrame) const;
    
    // Validation and error handling
    bool validatePipelineState(const ComputePipelineState& state) const;
    void logPipelineCreation(const ComputePipelineState& state, 
                           std::chrono::nanoseconds compilationTime) const;
    
    // Dispatch optimization helpers
    glm::uvec3 getDeviceOptimalWorkgroupSize() const;
    uint32_t getDeviceMaxComputeWorkgroupInvocations() const;
    bool deviceSupportsSubgroupOperations() const;
    
    // Barrier optimization
    bool canMergeBarriers(const VkBufferMemoryBarrier& a, const VkBufferMemoryBarrier& b) const;
    std::vector<VkBufferMemoryBarrier> optimizeBufferBarriers(const std::vector<VkBufferMemoryBarrier>& barriers) const;
};

// Utility functions for common compute patterns
namespace ComputePipelinePresets {
    // Entity movement computation (for your use case)
    ComputePipelineState createEntityMovementState(VkDescriptorSetLayout descriptorLayout);
    
    // Particle system update
    ComputePipelineState createParticleUpdateState(VkDescriptorSetLayout descriptorLayout);
    
    // Frustum culling
    ComputePipelineState createFrustumCullingState(VkDescriptorSetLayout descriptorLayout);
    
    // GPU sorting algorithms
    ComputePipelineState createRadixSortState(VkDescriptorSetLayout descriptorLayout);
    
    // Prefix sum (scan)
    ComputePipelineState createPrefixSumState(VkDescriptorSetLayout descriptorLayout);
}