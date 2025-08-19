#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include <chrono>
#include "../core/vulkan_raii.h"
#include "../core/vulkan_constants.h"
#include "graphics_pipeline_state_hash.h"

struct CachedGraphicsPipeline {
    vulkan_raii::Pipeline pipeline;
    vulkan_raii::PipelineLayout layout;
    GraphicsPipelineState state;
    uint64_t lastUsedFrame = 0;
    uint32_t useCount = 0;
    
    std::chrono::nanoseconds compilationTime{0};
    bool isHotPath = false;
};

struct PipelineStats {
    uint32_t totalPipelines = 0;
    uint32_t cacheHits = 0;
    uint32_t cacheMisses = 0;
    uint32_t compilationsThisFrame = 0;
    std::chrono::nanoseconds totalCompilationTime{0};
    float hitRatio = 0.0f;
};

class GraphicsPipelineCache {
public:
    explicit GraphicsPipelineCache(uint32_t maxSize = DEFAULT_GRAPHICS_CACHE_SIZE);
    ~GraphicsPipelineCache() = default;

    VkPipeline getPipeline(const GraphicsPipelineState& state);
    VkPipelineLayout getPipelineLayout(const GraphicsPipelineState& state);
    
    void storePipeline(const GraphicsPipelineState& state, std::unique_ptr<CachedGraphicsPipeline> pipeline);
    
    void clear();
    void optimizeCache(uint64_t currentFrame);
    void evictLeastRecentlyUsed();
    
    bool contains(const GraphicsPipelineState& state) const;
    size_t size() const { return cache_.size(); }
    
    const PipelineStats& getStats() const { return stats_; }
    void resetFrameStats();
    void updateStats(bool cacheHit, std::chrono::nanoseconds compilationTime = std::chrono::nanoseconds{0});
    
    void debugPrintCache() const;

private:
    std::unordered_map<GraphicsPipelineState, std::unique_ptr<CachedGraphicsPipeline>, GraphicsPipelineStateHash> cache_;
    
    uint32_t maxSize_;
    uint64_t cacheCleanupInterval_ = CACHE_CLEANUP_INTERVAL;
    
    mutable PipelineStats stats_;
    
    bool shouldEvictPipeline(const CachedGraphicsPipeline& pipeline, uint64_t currentFrame) const;
};