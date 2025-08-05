#pragma once

#include "component.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <algorithm>

// Change tracking for individual components
template<typename T>
class ComponentChangeTracker {
private:
    std::unordered_map<uint32_t, uint32_t> lastVersions;
    std::unordered_set<uint32_t> dirtyEntities;
    uint32_t frameVersion{0};
    
public:
    // Check if component has changed since last check
    bool hasChanged(uint32_t entityId, const T& component) {
        uint32_t currentVersion = getComponentVersion(component);
        auto it = lastVersions.find(entityId);
        
        if (it == lastVersions.end() || it->second != currentVersion) {
            lastVersions[entityId] = currentVersion;
            dirtyEntities.insert(entityId);
            return true;
        }
        
        return false;
    }
    
    // Mark entity as dirty
    void markDirty(uint32_t entityId) {
        dirtyEntities.insert(entityId);
    }
    
    // Check if entity is dirty
    bool isDirty(uint32_t entityId) const {
        return dirtyEntities.find(entityId) != dirtyEntities.end();
    }
    
    // Get all dirty entities for this frame
    const std::unordered_set<uint32_t>& getDirtyEntities() const {
        return dirtyEntities;
    }
    
    // Clear dirty entities (call after processing)
    void clearDirty() {
        dirtyEntities.clear();
        ++frameVersion;
    }
    
    // Remove entity from tracking
    void removeEntity(uint32_t entityId) {
        lastVersions.erase(entityId);
        dirtyEntities.erase(entityId);
    }
    
    // Statistics
    size_t getTrackedCount() const { return lastVersions.size(); }
    size_t getDirtyCount() const { return dirtyEntities.size(); }
    
private:
    uint32_t getComponentVersion(const T& component) {
        if constexpr (std::is_same_v<T, Transform>) {
            return component.dirty ? frameVersion + 1 : frameVersion;
        } else if constexpr (std::is_same_v<T, Renderable>) {
            return component.version;
        }
        return frameVersion; // Default for components without versioning
    }
};

// Global change detection system
class ChangeDetectionSystem {
private:
    ComponentChangeTracker<Transform> transformTracker;
    ComponentChangeTracker<Renderable> renderableTracker;
    ComponentChangeTracker<Velocity> velocityTracker;
    
    // Spatial change detection for rendering optimization
    struct SpatialCell {
        std::unordered_set<uint32_t> entities;
        bool dirty{false};
    };
    
    static constexpr float CELL_SIZE = 2.0f;
    std::unordered_map<uint64_t, SpatialCell> spatialGrid;
    std::unordered_map<uint32_t, uint64_t> entityCells;
    
public:
    // Check if any component of entity has changed
    bool hasEntityChanged(uint32_t entityId, const Transform* transform, 
                         const Renderable* renderable, const Velocity* velocity = nullptr) {
        bool changed = false;
        
        if (transform && transformTracker.hasChanged(entityId, *transform)) {
            changed = true;
            updateSpatialGrid(entityId, *transform);
        }
        
        if (renderable && renderableTracker.hasChanged(entityId, *renderable)) {
            changed = true;
        }
        
        if (velocity && velocityTracker.hasChanged(entityId, *velocity)) {
            changed = true;
        }
        
        return changed;
    }
    
    // Get entities that need rendering updates
    std::vector<uint32_t> getRenderDirtyEntities() {
        std::vector<uint32_t> dirty;
        
        // Combine transform and renderable changes
        for (uint32_t entityId : transformTracker.getDirtyEntities()) {
            dirty.push_back(entityId);
        }
        
        for (uint32_t entityId : renderableTracker.getDirtyEntities()) {
            if (std::find(dirty.begin(), dirty.end(), entityId) == dirty.end()) {
                dirty.push_back(entityId);
            }
        }
        
        return dirty;
    }
    
    // Get entities in spatial region (for frustum culling)
    std::vector<uint32_t> getEntitiesInRegion(float minX, float minY, float maxX, float maxY) {
        std::vector<uint32_t> entities;
        
        int32_t startCellX = static_cast<int32_t>(minX / CELL_SIZE);
        int32_t startCellY = static_cast<int32_t>(minY / CELL_SIZE);
        int32_t endCellX = static_cast<int32_t>(maxX / CELL_SIZE);
        int32_t endCellY = static_cast<int32_t>(maxY / CELL_SIZE);
        
        for (int32_t x = startCellX; x <= endCellX; ++x) {
            for (int32_t y = startCellY; y <= endCellY; ++y) {
                uint64_t cellKey = getCellKey(x, y);
                auto it = spatialGrid.find(cellKey);
                if (it != spatialGrid.end()) {
                    for (uint32_t entityId : it->second.entities) {
                        entities.push_back(entityId);
                    }
                }
            }
        }
        
        return entities;
    }
    
    // Frame cleanup
    void endFrame() {
        transformTracker.clearDirty();
        renderableTracker.clearDirty();
        velocityTracker.clearDirty();
        
        // Clear spatial grid dirty flags
        for (auto& [key, cell] : spatialGrid) {
            cell.dirty = false;
        }
    }
    
    // Entity removal
    void removeEntity(uint32_t entityId) {
        transformTracker.removeEntity(entityId);
        renderableTracker.removeEntity(entityId);
        velocityTracker.removeEntity(entityId);
        
        // Remove from spatial grid
        auto it = entityCells.find(entityId);
        if (it != entityCells.end()) {
            auto cellIt = spatialGrid.find(it->second);
            if (cellIt != spatialGrid.end()) {
                cellIt->second.entities.erase(entityId);
                if (cellIt->second.entities.empty()) {
                    spatialGrid.erase(cellIt);
                }
            }
            entityCells.erase(it);
        }
    }
    
    // Statistics
    struct ChangeStats {
        size_t transformTracked;
        size_t transformDirty;
        size_t renderableTracked;
        size_t renderableDirty;
        size_t velocityTracked;
        size_t velocityDirty;
        size_t spatialCells;
    };
    
    ChangeStats getStats() const {
        return {
            transformTracker.getTrackedCount(),
            transformTracker.getDirtyCount(),
            renderableTracker.getTrackedCount(),
            renderableTracker.getDirtyCount(),
            velocityTracker.getTrackedCount(),
            velocityTracker.getDirtyCount(),
            spatialGrid.size()
        };
    }
    
private:
    uint64_t getCellKey(int32_t x, int32_t y) {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }
    
    void updateSpatialGrid(uint32_t entityId, const Transform& transform) {
        int32_t cellX = static_cast<int32_t>(transform.position.x / CELL_SIZE);
        int32_t cellY = static_cast<int32_t>(transform.position.y / CELL_SIZE);
        uint64_t newCellKey = getCellKey(cellX, cellY);
        
        // Check if entity moved to different cell
        auto it = entityCells.find(entityId);
        if (it != entityCells.end() && it->second != newCellKey) {
            // Remove from old cell
            auto oldCellIt = spatialGrid.find(it->second);
            if (oldCellIt != spatialGrid.end()) {
                oldCellIt->second.entities.erase(entityId);
                if (oldCellIt->second.entities.empty()) {
                    spatialGrid.erase(oldCellIt);
                }
            }
        }
        
        // Add to new cell
        spatialGrid[newCellKey].entities.insert(entityId);
        spatialGrid[newCellKey].dirty = true;
        entityCells[entityId] = newCellKey;
    }
};