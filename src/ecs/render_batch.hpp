#pragma once

#include "component.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>

// Optimized render data structure for batch processing
struct RenderInstance {
    glm::mat4 transform;
    glm::vec4 color;
    uint32_t entityId; // For debugging/selection
    uint32_t layer;
    
    RenderInstance() = default;
    RenderInstance(const glm::mat4& t, const glm::vec4& c, uint32_t id, uint32_t l)
        : transform(t), color(c), entityId(id), layer(l) {}
};

// Batch container for each shape type
class RenderBatch {
private:
    std::vector<RenderInstance> instances;
    bool needsSorting{false};
    uint32_t lastVersion{0};
    
public:
    void reserve(size_t capacity) {
        instances.reserve(capacity);
    }
    
    void clear() {
        instances.clear();
        needsSorting = false;
    }
    
    void addInstance(const RenderInstance& instance) {
        instances.push_back(instance);
        needsSorting = true;
    }
    
    void addInstance(const glm::mat4& transform, const glm::vec4& color, 
                    uint32_t entityId, uint32_t layer) {
        instances.emplace_back(transform, color, entityId, layer);
        needsSorting = true;
    }
    
    // Sort by layer for proper depth ordering
    void sort() {
        if (needsSorting && !instances.empty()) {
            std::sort(instances.begin(), instances.end(), 
                     [](const RenderInstance& a, const RenderInstance& b) {
                         return a.layer < b.layer;
                     });
            needsSorting = false;
        }
    }
    
    const std::vector<RenderInstance>& getInstances() const {
        return instances;
    }
    
    size_t getInstanceCount() const {
        return instances.size();
    }
    
    bool empty() const {
        return instances.empty();
    }
    
    // Memory-efficient access to transform matrices for GPU upload  
    const float* getTransformData() const {
        return instances.empty() ? nullptr : &instances[0].transform[0][0];
    }
    
    // Memory-efficient access to color data for GPU upload
    const float* getColorData() const {
        return instances.empty() ? nullptr : &instances[0].color[0];
    }
    
    // Get stride for interleaved data
    static constexpr size_t getInstanceStride() {
        return sizeof(RenderInstance);
    }
};

// Batch manager for all render types
class BatchRenderer {
private:
    // Single unified batch - GPU treats everything as triangles
    RenderBatch batch;
    
    // Change tracking
    uint32_t frameVersion{0};
    uint32_t lastProcessedVersion{0};
    
public:
    // Performance statistics
    struct Stats {
        size_t totalEntities{0};
        size_t visibleEntities{0};
        size_t batchCount{0};
        float lastUpdateTime{0.0f};
    };
    
private:
    Stats stats;
    
public:
    BatchRenderer() {
        // Pre-allocate for thousands of entities
        batch.reserve(10000);
    }
    
    void beginFrame() {
        ++frameVersion;
        
        // Clear batch
        batch.clear();
        
        stats.totalEntities = 0;
        stats.visibleEntities = 0;
        stats.batchCount = 0;
    }
    
    void addEntity(const Transform& transform, const Renderable& renderable, uint32_t entityId) {
        ++stats.totalEntities;
        
        if (!renderable.visible) {
            return;
        }
        
        ++stats.visibleEntities;
        
        batch.addInstance(
            transform.getMatrix(),
            renderable.color,
            entityId,
            renderable.layer
        );
    }
    
    void endFrame() {
        // Sort batch by layer
        if (!batch.empty()) {
            batch.sort();
            stats.batchCount = 1;
        } else {
            stats.batchCount = 0;
        }
        
        lastProcessedVersion = frameVersion;
    }
    
    // Get the unified batch
    const RenderBatch& getBatch() const {
        return batch;
    }
    
    // Check if batch has data
    bool hasRenderData() const {
        return !batch.empty();
    }
    
    // Get performance statistics
    const Stats& getStats() const {
        return stats;
    }
    
    // Process the unified batch
    template<typename Func>
    void processBatch(Func&& func) const {
        if (!batch.empty()) {
            func(batch);
        }
    }
    
    // Get total instance count
    size_t getTotalInstanceCount() const {
        return batch.getInstanceCount();
    }
    
    // Memory usage estimation
    size_t getMemoryUsage() const {
        return batch.getInstanceCount() * sizeof(RenderInstance);
    }
};