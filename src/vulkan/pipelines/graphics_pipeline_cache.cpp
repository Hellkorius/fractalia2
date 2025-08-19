#include "graphics_pipeline_cache.h"
#include <iostream>
#include <algorithm>

GraphicsPipelineCache::GraphicsPipelineCache(uint32_t maxSize) : maxSize_(maxSize) {
}

VkPipeline GraphicsPipelineCache::getPipeline(const GraphicsPipelineState& state) {
    auto it = cache_.find(state);
    if (it != cache_.end()) {
        stats_.cacheHits++;
        it->second->lastUsedFrame = stats_.cacheHits + stats_.cacheMisses;
        it->second->useCount++;
        return it->second->pipeline.get();
    }
    
    stats_.cacheMisses++;
    return VK_NULL_HANDLE;
}

VkPipelineLayout GraphicsPipelineCache::getPipelineLayout(const GraphicsPipelineState& state) {
    auto it = cache_.find(state);
    if (it != cache_.end()) {
        return it->second->layout.get();
    }
    return VK_NULL_HANDLE;
}

void GraphicsPipelineCache::storePipeline(const GraphicsPipelineState& state, std::unique_ptr<CachedGraphicsPipeline> pipeline) {
    stats_.totalPipelines++;
    stats_.compilationsThisFrame++;
    
    if (pipeline->compilationTime.count() > 0) {
        stats_.totalCompilationTime += pipeline->compilationTime;
    }
    
    cache_[state] = std::move(pipeline);
    
    if (cache_.size() > maxSize_) {
        evictLeastRecentlyUsed();
    }
}

void GraphicsPipelineCache::clear() {
    // Clear cache in dependency order - pipelines first, then layouts
    cache_.clear();
    
    // Reset statistics to prevent corruption
    stats_.totalPipelines = 0;
    stats_.cacheHits = 0;
    stats_.cacheMisses = 0;
    stats_.compilationsThisFrame = 0;
    stats_.totalCompilationTime = std::chrono::nanoseconds{0};
    stats_.hitRatio = 0.0f;
}

void GraphicsPipelineCache::optimizeCache(uint64_t currentFrame) {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (shouldEvictPipeline(*it->second, currentFrame)) {
            it = cache_.erase(it);
            stats_.totalPipelines--;
        } else {
            ++it;
        }
    }
}

void GraphicsPipelineCache::evictLeastRecentlyUsed() {
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

bool GraphicsPipelineCache::contains(const GraphicsPipelineState& state) const {
    return cache_.find(state) != cache_.end();
}

void GraphicsPipelineCache::resetFrameStats() {
    stats_.compilationsThisFrame = 0;
    stats_.hitRatio = static_cast<float>(stats_.cacheHits) / static_cast<float>(stats_.cacheHits + stats_.cacheMisses);
}

void GraphicsPipelineCache::updateStats(bool cacheHit, std::chrono::nanoseconds compilationTime) {
    if (cacheHit) {
        stats_.cacheHits++;
    } else {
        stats_.cacheMisses++;
        stats_.compilationsThisFrame++;
        if (compilationTime.count() > 0) {
            stats_.totalCompilationTime += compilationTime;
        }
    }
}

void GraphicsPipelineCache::debugPrintCache() const {
    std::cout << "Graphics Pipeline Cache Statistics:" << std::endl;
    std::cout << "  Total pipelines: " << stats_.totalPipelines << std::endl;
    std::cout << "  Cache hits: " << stats_.cacheHits << std::endl;
    std::cout << "  Cache misses: " << stats_.cacheMisses << std::endl;
    std::cout << "  Hit ratio: " << stats_.hitRatio << std::endl;
    std::cout << "  Compilations this frame: " << stats_.compilationsThisFrame << std::endl;
    std::cout << "  Total compilation time: " << stats_.totalCompilationTime.count() / 1000000.0f << "ms" << std::endl;
}

bool GraphicsPipelineCache::shouldEvictPipeline(const CachedGraphicsPipeline& pipeline, uint64_t currentFrame) const {
    return currentFrame - pipeline.lastUsedFrame > cacheCleanupInterval_;
}