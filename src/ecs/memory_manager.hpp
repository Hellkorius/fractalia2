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
#include <mutex>
#include <new>
#include <bitset>

// Block-based allocator with stable pointers for Flecs integration
template<typename T>
class BlockAllocator {
private:
    static constexpr size_t BLOCK_SIZE = 512; // Components per block
    static constexpr size_t COMPONENTS_PER_BLOCK = BLOCK_SIZE / sizeof(T) > 0 ? BLOCK_SIZE / sizeof(T) : 1;
    
    struct Block {
        alignas(T) char data[COMPONENTS_PER_BLOCK * sizeof(T)];
        std::bitset<COMPONENTS_PER_BLOCK> occupied;
        size_t freeCount{COMPONENTS_PER_BLOCK};
        
        T* get(size_t index) {
            return reinterpret_cast<T*>(data + index * sizeof(T));
        }
        
        size_t findFreeSlot() {
            for (size_t i = 0; i < COMPONENTS_PER_BLOCK; ++i) {
                if (!occupied[i]) {
                    return i;
                }
            }
            return COMPONENTS_PER_BLOCK; // Not found
        }
    };
    
    std::vector<std::unique_ptr<Block>> blocks;
    mutable std::mutex mutex; // Thread-safety for multi-threaded Flecs
    size_t totalAllocated{0};
    size_t totalCapacity{0};
    
public:
    BlockAllocator() {
        // Pre-allocate first block
        addBlock();
    }
    
    ~BlockAllocator() {
        clear();
    }
    
    // Thread-safe allocation with stable pointers
    template<typename... Args>
    T* allocate(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Find a block with free space
        for (auto& block : blocks) {
            if (block->freeCount > 0) {
                size_t slot = block->findFreeSlot();
                if (slot < COMPONENTS_PER_BLOCK) {
                    T* ptr = block->get(slot);
                    new(ptr) T(std::forward<Args>(args)...);
                    block->occupied[slot] = true;
                    block->freeCount--;
                    totalAllocated++;
                    return ptr;
                }
            }
        }
        
        // No free space, add new block
        addBlock();
        auto& newBlock = blocks.back();
        T* ptr = newBlock->get(0);
        new(ptr) T(std::forward<Args>(args)...);
        newBlock->occupied[0] = true;
        newBlock->freeCount--;
        totalAllocated++;
        return ptr;
    }
    
    // Thread-safe deallocation
    void deallocate(T* component) {
        if (!component) return;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Find which block contains this pointer
        for (auto& block : blocks) {
            char* blockStart = block->data;
            char* blockEnd = blockStart + sizeof(block->data);
            char* ptr = reinterpret_cast<char*>(component);
            
            if (ptr >= blockStart && ptr < blockEnd) {
                size_t slot = (ptr - blockStart) / sizeof(T);
                if (slot < COMPONENTS_PER_BLOCK && block->occupied[slot]) {
                    component->~T();
                    block->occupied[slot] = false;
                    block->freeCount++;
                    totalAllocated--;
                    return;
                }
            }
        }
    }
    
    // Clear all components (not thread-safe, call during shutdown)
    void clear() {
        for (auto& block : blocks) {
            for (size_t i = 0; i < COMPONENTS_PER_BLOCK; ++i) {
                if (block->occupied[i]) {
                    block->get(i)->~T();
                }
            }
        }
        blocks.clear();
        totalAllocated = 0;
        totalCapacity = 0;
    }
    
    // Statistics (thread-safe)
    size_t getMemoryUsage() const {
        std::lock_guard<std::mutex> lock(mutex);
        return totalCapacity;
    }
    
    size_t getAllocatedCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return totalAllocated;
    }
    
    size_t getCapacity() const {
        std::lock_guard<std::mutex> lock(mutex);
        return totalCapacity;
    }
    
private:
    void addBlock() {
        blocks.push_back(std::make_unique<Block>());
        totalCapacity += COMPONENTS_PER_BLOCK * sizeof(T);
    }
};

/*
 * UNIFIED STORAGE ARCHITECTURE
 * 
 * This design eliminates double-tracking by making Flecs the sole authority for component storage:
 * 
 * - Components are created/destroyed via standard Flecs APIs (entity.set<T>(), entity.remove<T>())
 * - BlockAllocator<T> instances are maintained for potential future manual memory management
 * - ECSMemoryManager tracks Flecs component counts as the authoritative source
 * - No parallel component pools - eliminates memory duplication and consistency issues
 * 
 * Benefits:
 * - Single source of truth (Flecs tables)  
 * - No double-tracking overhead
 * - Simplified API - just use entity.set<T>()
 * - Block allocators available for specialized use cases
 * - Thread-safe allocators ready if needed
 */

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

// Comprehensive memory manager for ECS with Flecs integration
class ECSMemoryManager {
private:
    // World reference for component counting
    flecs::world& world;
    
    // Block allocators for stable pointer components
    std::unique_ptr<BlockAllocator<Transform>> transformAllocator;
    std::unique_ptr<BlockAllocator<Renderable>> renderableAllocator;
    std::unique_ptr<BlockAllocator<Velocity>> velocityAllocator;
    std::unique_ptr<BlockAllocator<Bounds>> boundsAllocator;
    std::unique_ptr<BlockAllocator<Lifetime>> lifetimeAllocator;
    
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
        : world(world),
          transformAllocator(std::make_unique<BlockAllocator<Transform>>()),
          renderableAllocator(std::make_unique<BlockAllocator<Renderable>>()),
          velocityAllocator(std::make_unique<BlockAllocator<Velocity>>()),
          boundsAllocator(std::make_unique<BlockAllocator<Bounds>>()),
          lifetimeAllocator(std::make_unique<BlockAllocator<Lifetime>>()),
          entityRecycler(std::make_unique<EntityRecycler>(world)) {
        // Note: No setup needed - Flecs handles components natively
    }
    
    ~ECSMemoryManager() = default;
    
    // Entity management
    Entity createEntity() {
        return entityRecycler->acquire();
    }
    
    void destroyEntity(Entity entity) {
        entityRecycler->release(entity);
    }
    
private:
    // Allocators are available for manual use but Flecs manages components natively
    // This eliminates double-tracking while preserving allocator infrastructure
    
public:
    // Components are now allocated automatically through Flecs hooks
    // No need for manual allocation/deallocation methods
    // Just use entity.set<T>() and entity.remove<T>() as normal
    
    // Maintenance operations
    void cleanup() {
        entityRecycler->cleanup();
        updateStats();
    }
    
    void clearAllocators() {
        transformAllocator->clear();
        renderableAllocator->clear();
        velocityAllocator->clear();
        boundsAllocator->clear();  
        lifetimeAllocator->clear();
    }
    
    // Memory statistics
    const MemoryStats& getStats() const { return stats; }
    
    void updateStats() {
        // Use Flecs component counts for consistent reporting
        size_t transformCount = static_cast<size_t>(world.count<Transform>());
        size_t renderableCount = static_cast<size_t>(world.count<Renderable>());
        size_t velocityCount = static_cast<size_t>(world.count<Velocity>());
        size_t boundsCount = static_cast<size_t>(world.count<Bounds>());
        size_t lifetimeCount = static_cast<size_t>(world.count<Lifetime>());
        size_t movementPatternCount = static_cast<size_t>(world.count<MovementPattern>());
        
        stats.totalAllocated = 
            transformCount * sizeof(Transform) +
            renderableCount * sizeof(Renderable) +
            velocityCount * sizeof(Velocity) +
            boundsCount * sizeof(Bounds) +
            lifetimeCount * sizeof(Lifetime) +
            movementPatternCount * sizeof(MovementPattern);
            
        // Block allocators are now reserved/available memory, not actively tracking
        stats.totalCapacity = stats.totalAllocated; // Flecs manages actual capacity
            
        stats.entityPoolSize = entityRecycler->getPoolSize();
        stats.activeEntities = transformCount; // Use Transform count as entity proxy
    }
    
    // Configuration
    void configureEntityRecycler(size_t maxPoolSize, std::chrono::seconds maxAge) {
        entityRecycler->setMaxPoolSize(maxPoolSize);
        entityRecycler->setMaxAge(maxAge);
    }
    
    // Get unified memory statistics (Flecs is authoritative)
    struct UnifiedMemoryStats {
        size_t transformCount, renderableCount, velocityCount, boundsCount, lifetimeCount, movementPatternCount;
        size_t entityPoolSize;
        size_t totalComponentMemory;
        size_t allocatorReservedMemory; // Available but unused allocator capacity
    };
    
    UnifiedMemoryStats getUnifiedStats() const {
        size_t transformCount = static_cast<size_t>(world.count<Transform>());
        size_t renderableCount = static_cast<size_t>(world.count<Renderable>());
        size_t velocityCount = static_cast<size_t>(world.count<Velocity>());
        size_t boundsCount = static_cast<size_t>(world.count<Bounds>());
        size_t lifetimeCount = static_cast<size_t>(world.count<Lifetime>());
        size_t movementPatternCount = static_cast<size_t>(world.count<MovementPattern>());
        
        size_t totalComponentMemory = 
            transformCount * sizeof(Transform) +
            renderableCount * sizeof(Renderable) +
            velocityCount * sizeof(Velocity) +
            boundsCount * sizeof(Bounds) +
            lifetimeCount * sizeof(Lifetime) +
            movementPatternCount * sizeof(MovementPattern);
            
        size_t allocatorReservedMemory =
            transformAllocator->getMemoryUsage() +
            renderableAllocator->getMemoryUsage() +
            velocityAllocator->getMemoryUsage() +
            boundsAllocator->getMemoryUsage() +
            lifetimeAllocator->getMemoryUsage();
        
        return {
            transformCount, renderableCount, velocityCount, boundsCount, lifetimeCount, movementPatternCount,
            entityRecycler->getPoolSize(),
            totalComponentMemory,
            allocatorReservedMemory
        };
    }
    
    // Debug information for unified storage
    void printMemoryReport() const {
        auto unified = getUnifiedStats();
        printf("ECS Memory Report (Unified Flecs Storage):\n");
        printf("  Entity Pool Size: %zu\n", unified.entityPoolSize);
        printf("  \n");
        printf("  Active Component Counts (Flecs authoritative):\n");
        printf("    Transform: %zu (%zu bytes)\n", unified.transformCount, unified.transformCount * sizeof(Transform));
        printf("    Renderable: %zu (%zu bytes)\n", unified.renderableCount, unified.renderableCount * sizeof(Renderable));
        printf("    Velocity: %zu (%zu bytes)\n", unified.velocityCount, unified.velocityCount * sizeof(Velocity));
        printf("    Bounds: %zu (%zu bytes)\n", unified.boundsCount, unified.boundsCount * sizeof(Bounds));
        printf("    Lifetime: %zu (%zu bytes)\n", unified.lifetimeCount, unified.lifetimeCount * sizeof(Lifetime));
        printf("    MovementPattern: %zu (%zu bytes)\n", unified.movementPatternCount, unified.movementPatternCount * sizeof(MovementPattern));
        printf("  \n");
        printf("  Total Component Memory: %zu bytes\n", unified.totalComponentMemory);
        printf("  Reserved Allocator Memory: %zu bytes (available for future use)\n", unified.allocatorReservedMemory);
        printf("  \n");
        printf("  ✓ UNIFIED STORAGE: No double-tracking - Flecs is sole authority\n");
    }
};