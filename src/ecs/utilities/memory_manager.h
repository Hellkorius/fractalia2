#pragma once

#include "../components/entity.h"
#include "../components/component.h"
#include <flecs.h>

// Minimal memory statistics helper for ECS
// Uses Flecs as the single source of truth for all component data
class ECSMemoryManager {
private:
    flecs::world& world;
    
public:
    ECSMemoryManager(flecs::world& world) : world(world) {}
    ~ECSMemoryManager() = default;
    
    // Simple memory statistics
    struct MemoryStats {
        size_t transformCount{0};
        size_t renderableCount{0};
        size_t velocityCount{0};
        size_t boundsCount{0};
        size_t lifetimeCount{0};
        size_t movementPatternCount{0};
        size_t totalComponentMemory{0};
        size_t activeEntities{0};
    };
    
    // Get current memory statistics from Flecs
    MemoryStats getStats() const {
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
        
        return {
            transformCount, renderableCount, velocityCount, boundsCount, lifetimeCount, movementPatternCount,
            totalComponentMemory,
            transformCount // Use Transform count as proxy for active entities
        };
    }
    
    // Debug information
    void printMemoryReport() const {
        auto stats = getStats();
        printf("ECS Memory Report (Flecs Native Storage):\n");
        printf("  Active Component Counts:\n");
        printf("    Transform: %zu (%zu bytes)\n", stats.transformCount, stats.transformCount * sizeof(Transform));
        printf("    Renderable: %zu (%zu bytes)\n", stats.renderableCount, stats.renderableCount * sizeof(Renderable));
        printf("    Velocity: %zu (%zu bytes)\n", stats.velocityCount, stats.velocityCount * sizeof(Velocity));
        printf("    Bounds: %zu (%zu bytes)\n", stats.boundsCount, stats.boundsCount * sizeof(Bounds));
        printf("    Lifetime: %zu (%zu bytes)\n", stats.lifetimeCount, stats.lifetimeCount * sizeof(Lifetime));
        printf("    MovementPattern: %zu (%zu bytes)\n", stats.movementPatternCount, stats.movementPatternCount * sizeof(MovementPattern));
        printf("  \n");
        printf("  Total Component Memory: %zu bytes (%zu KB)\n", stats.totalComponentMemory, stats.totalComponentMemory / 1024);
        printf("  Active Entities: %zu\n", stats.activeEntities);
        printf("  \n");
        printf("  ✓ SIMPLE: Flecs is sole authority - no custom allocators\n");
    }
};