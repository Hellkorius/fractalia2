#pragma once

#include "systems_common.h"

/**
 * @brief Movement systems for entity movement patterns and physics
 * 
 * Provides functions to register movement-related systems with Flecs world:
 * - Movement pattern updates (random walk)
 * - Physics integration (velocity application)
 * - Movement synchronization
 */
namespace MovementSystem {
    /**
     * @brief Register all movement systems with the Flecs world
     * @param world The Flecs world instance
     * @param gpuManager Optional GPU entity manager for synchronization
     */
    void registerSystems(flecs::world& world, GPUEntityManager* gpuManager = nullptr);
    
    /**
     * @brief Setup movement execution phases
     * @param world The Flecs world instance
     */
    void setupMovementPhases(flecs::world& world);
    
    /**
     * @brief Reset all movement patterns to initial state
     * @param world The Flecs world instance
     */
    void resetAllMovementPatterns(flecs::world& world);
    
    /**
     * @brief Get movement statistics for monitoring
     */
    struct MovementStats {
        size_t entitiesWithMovement = 0;
        size_t entitiesWithPhysics = 0;
        float lastUpdateTime = 0.0f;
        float averageUpdateTime = 0.0f;
    };
    
    const MovementStats& getStats();
    void resetStats();
}