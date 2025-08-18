#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include "compute_pipeline_types.h"

class ComputePipelineCache {
public:
    explicit ComputePipelineCache(uint32_t maxSize = 512);
    ~ComputePipelineCache() = default;

    struct Stats {
        uint32_t totalPipelines = 0;
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        std::chrono::nanoseconds totalCompilationTime{0};
        float hitRatio = 0.0f;
    };

    VkPipeline getPipeline(const ComputePipelineState& state);
    VkPipelineLayout getPipelineLayout(const ComputePipelineState& state);
    
    bool contains(const ComputePipelineState& state) const;
    void insert(const ComputePipelineState& state, std::unique_ptr<CachedComputePipeline> pipeline);
    
    void optimizeCache(uint64_t currentFrame);
    void clear();
    
    Stats getStats() const { return stats_; }
    void resetFrameStats();
    
    void setCreatePipelineCallback(std::function<std::unique_ptr<CachedComputePipeline>(const ComputePipelineState&)> callback);

private:
    std::unordered_map<ComputePipelineState, std::unique_ptr<CachedComputePipeline>, ComputePipelineStateHash> cache_;
    std::function<std::unique_ptr<CachedComputePipeline>(const ComputePipelineState&)> createPipelineCallback_;
    
    uint32_t maxSize_;
    uint64_t frameCounter_ = 0;
    mutable Stats stats_;
    
    void evictLeastRecentlyUsed();
    void updateStats(bool isHit, std::chrono::nanoseconds compilationTime = {});
};