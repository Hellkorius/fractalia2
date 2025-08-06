#pragma once

#include "entity.hpp"
#include "component.hpp"
#include <vector>
#include <queue>
#include <memory>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <chrono>
#include <cstdio>

// Memory pool for efficient component allocation
template<typename T>
class ComponentPool {
private:
    std::vector<T> components;
    std::queue<size_t> freeIndices;
    size_t nextIndex{0};
    
public:
    ComponentPool(size_t initialCapacity = 1000) {
        components.reserve(initialCapacity);
    }
    
    // Allocate component with perfect forwarding
    template<typename... Args>
    T* allocate(Args&&... args) {
        size_t index;
        
        if (!freeIndices.empty()) {
            index = freeIndices.front();
            freeIndices.pop();
            components[index] = T(std::forward<Args>(args)...);
        } else {
            if (nextIndex >= components.capacity()) {
                components.reserve(components.capacity() * 2);
            }
            
            index = nextIndex++;
            if (index >= components.size()) {
                components.resize(index + 1);
            }
            components[index] = T(std::forward<Args>(args)...);
        }
        
        return &components[index];
    }
    
    // Deallocate component
    void deallocate(T* component) {
        if (component >= components.data() && 
            component < components.data() + components.size()) {
            size_t index = component - components.data();
            freeIndices.push(index);
        }
    }
    
    // Get component by index
    T* get(size_t index) {
        return index < components.size() ? &components[index] : nullptr;
    }
    
    // Clear all components
    void clear() {
        components.clear();
        while (!freeIndices.empty()) {
            freeIndices.pop();
        }
        nextIndex = 0;
    }
    
    // Memory usage statistics
    size_t getMemoryUsage() const {
        return components.capacity() * sizeof(T);
    }
    
    size_t getAllocatedCount() const {
        return nextIndex - freeIndices.size();
    }
    
    size_t getCapacity() const {
        return components.capacity();
    }
};

// Entity recycling manager
class EntityRecycler {
private:
    flecs::world& world;
    std::vector<Entity> recyclePool;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> entityAges;
    
    // Configuration
    size_t maxPoolSize{5000};
    std::chrono::seconds maxAge{60}; // Recycle entities after 60 seconds
    
public:
    EntityRecycler(flecs::world& w) : world(w) {
        recyclePool.reserve(maxPoolSize);
    }
    
    // Get entity from pool or create new one
    Entity acquire() {
        // Clean up old entities first
        cleanup();
        
        Entity entity;
        if (!recyclePool.empty()) {
            entity = recyclePool.back();
            recyclePool.pop_back();
            
            // Clear all components
            entity.clear();
        } else {
            entity = world.entity();
        }
        
        // Track creation time
        entityAges[static_cast<uint32_t>(entity.id())] = std::chrono::steady_clock::now();
        
        return entity;
    }
    
    // Return entity to pool
    void release(Entity entity) {
        if (!entity.is_valid()) return;
        
        if (recyclePool.size() < maxPoolSize) {
            // Clear components but keep entity alive
            entity.clear();
            recyclePool.push_back(entity);
        } else {
            // Pool is full, destroy entity
            entity.destruct();
            entityAges.erase(static_cast<uint32_t>(entity.id()));
        }
    }
    
    // Force cleanup of old entities
    void cleanup() {
        auto now = std::chrono::steady_clock::now();
        
        recyclePool.erase(
            std::remove_if(recyclePool.begin(), recyclePool.end(),
                [&](const Entity& entity) {
                    uint32_t id = static_cast<uint32_t>(entity.id());
                    auto it = entityAges.find(id);
                    if (it != entityAges.end() && (now - it->second) > maxAge) {
                        entity.destruct();
                        entityAges.erase(it);
                        return true;
                    }
                    return false;
                }),
            recyclePool.end()
        );
    }
    
    // Configuration
    void setMaxPoolSize(size_t size) { maxPoolSize = size; }
    void setMaxAge(std::chrono::seconds age) { maxAge = age; }
    
    // Statistics
    size_t getPoolSize() const { return recyclePool.size(); }
    size_t getMaxPoolSize() const { return maxPoolSize; }
    size_t getTrackedEntityCount() const { return entityAges.size(); }
};

// Comprehensive memory manager for ECS
class ECSMemoryManager {
private:
    // Component pools
    ComponentPool<Transform> transformPool;
    ComponentPool<Renderable> renderablePool;
    ComponentPool<Velocity> velocityPool;
    ComponentPool<Bounds> boundsPool;
    ComponentPool<Lifetime> lifetimePool;
    
    // Entity recycling
    std::unique_ptr<EntityRecycler> entityRecycler;
    
public:
    // Memory tracking
    struct MemoryStats {
        size_t totalAllocated{0};
        size_t totalCapacity{0};
        size_t entityPoolSize{0};
        size_t activeEntities{0};
    };
    
private:
    MemoryStats stats;
    
public:
    ECSMemoryManager(flecs::world& world, size_t initialCapacity = 10000) 
        : transformPool(initialCapacity),
          renderablePool(initialCapacity),
          velocityPool(initialCapacity),
          boundsPool(initialCapacity),
          lifetimePool(initialCapacity),
          entityRecycler(std::make_unique<EntityRecycler>(world)) {}
    
    // Entity management
    Entity createEntity() {
        return entityRecycler->acquire();
    }
    
    void destroyEntity(Entity entity) {
        entityRecycler->release(entity);
    }
    
    // Component allocation (custom allocators for better performance)
    template<typename T, typename... Args>
    T* allocateComponent(Args&&... args) {
        static_assert(std::is_same_v<T, Transform> || 
                     std::is_same_v<T, Renderable> ||
                     std::is_same_v<T, Velocity> ||
                     std::is_same_v<T, Bounds> ||
                     std::is_same_v<T, Lifetime>, 
                     "Component type not supported by memory manager");
        
        if constexpr (std::is_same_v<T, Transform>) {
            return transformPool.allocate(std::forward<Args>(args)...);
        } else if constexpr (std::is_same_v<T, Renderable>) {
            return renderablePool.allocate(std::forward<Args>(args)...);
        } else if constexpr (std::is_same_v<T, Velocity>) {
            return velocityPool.allocate(std::forward<Args>(args)...);
        } else if constexpr (std::is_same_v<T, Bounds>) {
            return boundsPool.allocate(std::forward<Args>(args)...);
        } else if constexpr (std::is_same_v<T, Lifetime>) {
            return lifetimePool.allocate(std::forward<Args>(args)...);
        }
        return nullptr;
    }
    
    template<typename T>
    void deallocateComponent(T* component) {
        if constexpr (std::is_same_v<T, Transform>) {
            transformPool.deallocate(component);
        } else if constexpr (std::is_same_v<T, Renderable>) {
            renderablePool.deallocate(component);
        } else if constexpr (std::is_same_v<T, Velocity>) {
            velocityPool.deallocate(component);
        } else if constexpr (std::is_same_v<T, Bounds>) {
            boundsPool.deallocate(component);
        } else if constexpr (std::is_same_v<T, Lifetime>) {
            lifetimePool.deallocate(component);
        }
    }
    
    // Maintenance operations
    void cleanup() {
        entityRecycler->cleanup();
        updateStats();
    }
    
    void clearPools() {
        transformPool.clear();
        renderablePool.clear();
        velocityPool.clear();
        boundsPool.clear();  
        lifetimePool.clear();
    }
    
    // Memory statistics
    const MemoryStats& getStats() const { return stats; }
    
    void updateStats() {
        stats.totalAllocated = 
            transformPool.getAllocatedCount() * sizeof(Transform) +
            renderablePool.getAllocatedCount() * sizeof(Renderable) +
            velocityPool.getAllocatedCount() * sizeof(Velocity) +
            boundsPool.getAllocatedCount() * sizeof(Bounds) +
            lifetimePool.getAllocatedCount() * sizeof(Lifetime);
            
        stats.totalCapacity =
            transformPool.getMemoryUsage() +
            renderablePool.getMemoryUsage() +
            velocityPool.getMemoryUsage() +
            boundsPool.getMemoryUsage() +
            lifetimePool.getMemoryUsage();
            
        stats.entityPoolSize = entityRecycler->getPoolSize();
        // Note: activeEntities will be set by World::getStats() with proper Flecs count
    }
    
    // Configuration
    void configureEntityRecycler(size_t maxPoolSize, std::chrono::seconds maxAge) {
        entityRecycler->setMaxPoolSize(maxPoolSize);
        entityRecycler->setMaxAge(maxAge);
    }
    
    // Debug information
    void printMemoryReport() const {
        printf("ECS Memory Report:\n");
        printf("  Total Allocated: %zu bytes\n", stats.totalAllocated);
        printf("  Total Capacity: %zu bytes\n", stats.totalCapacity);
        printf("  Entity Pool Size: %zu\n", stats.entityPoolSize);
        printf("  Active Entities: %zu\n", stats.activeEntities);
        printf("  Memory Efficiency: %.2f%%\n", 
               stats.totalCapacity > 0 ? (float)stats.totalAllocated / stats.totalCapacity * 100.0f : 0.0f);
    }
};