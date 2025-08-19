#include "compute_pipeline_cache.h"
#include <iostream>
#include <algorithm>

ComputePipelineCache::ComputePipelineCache(uint32_t maxSize) : maxSize_(maxSize) {}

VkPipeline ComputePipelineCache::getPipeline(const ComputePipelineState& state) {
    auto it = cache_.find(state);
    if (it != cache_.end()) {
        updateStats(true);
        it->second->lastUsedFrame = ++frameCounter_;
        it->second->useCount++;
        return it->second->pipeline.get();
    }
    
    updateStats(false);
    
    if (!createPipelineCallback_) {
        std::cerr << "ComputePipelineCache: No create pipeline callback set" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    auto cachedPipeline = createPipelineCallback_(state);
    if (!cachedPipeline) {
        return VK_NULL_HANDLE;
    }
    
    VkPipeline pipeline = cachedPipeline->pipeline.get();
    cachedPipeline->lastUsedFrame = ++frameCounter_;
    
    updateStats(false, cachedPipeline->compilationTime);
    
    cache_[state] = std::move(cachedPipeline);
    stats_.totalPipelines++;
    
    if (cache_.size() > maxSize_) {
        evictLeastRecentlyUsed();
    }
    
    return pipeline;
}

VkPipelineLayout ComputePipelineCache::getPipelineLayout(const ComputePipelineState& state) {
    auto it = cache_.find(state);
    if (it != cache_.end()) {
        return it->second->layout.get();
    }
    
    VkPipeline pipeline = getPipeline(state);
    if (pipeline == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    it = cache_.find(state);
    return it != cache_.end() ? it->second->layout.get() : VK_NULL_HANDLE;
}

bool ComputePipelineCache::contains(const ComputePipelineState& state) const {
    return cache_.find(state) != cache_.end();
}

void ComputePipelineCache::insert(const ComputePipelineState& state, std::unique_ptr<CachedComputePipeline> pipeline) {
    pipeline->lastUsedFrame = ++frameCounter_;
    updateStats(false, pipeline->compilationTime);
    
    cache_[state] = std::move(pipeline);
    stats_.totalPipelines++;
    
    if (cache_.size() > maxSize_) {
        evictLeastRecentlyUsed();
    }
}

void ComputePipelineCache::optimizeCache(uint64_t currentFrame) {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (currentFrame - it->second->lastUsedFrame > CACHE_CLEANUP_INTERVAL) {
            it = cache_.erase(it);
            stats_.totalPipelines--;
        } else {
            ++it;
        }
    }
}

void ComputePipelineCache::clear() {
    // Clear cache in dependency order - pipelines first, then layouts
    cache_.clear();
    
    // Reset statistics to prevent corruption
    stats_.totalPipelines = 0;
    stats_.cacheHits = 0;
    stats_.cacheMisses = 0;
    stats_.totalCompilationTime = std::chrono::nanoseconds{0};
    stats_.hitRatio = 0.0f;
}

void ComputePipelineCache::resetFrameStats() {
    stats_.hitRatio = static_cast<float>(stats_.cacheHits) / static_cast<float>(stats_.cacheHits + stats_.cacheMisses);
}

void ComputePipelineCache::setCreatePipelineCallback(std::function<std::unique_ptr<CachedComputePipeline>(const ComputePipelineState&)> callback) {
    createPipelineCallback_ = std::move(callback);
}

void ComputePipelineCache::evictLeastRecentlyUsed() {
    if (cache_.empty()) return;
    
    auto lruIt = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second->lastUsedFrame < lruIt->second->lastUsedFrame) {
            lruIt = it;
        }
    }
    
    cache_.erase(lruIt);
    stats_.totalPipelines--;
}

void ComputePipelineCache::updateStats(bool isHit, std::chrono::nanoseconds compilationTime) {
    if (isHit) {
        stats_.cacheHits++;
    } else {
        stats_.cacheMisses++;
        stats_.totalCompilationTime += compilationTime;
    }
}