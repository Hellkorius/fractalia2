#pragma once

#include "systems_common.h"

/**
 * @brief Movement system infrastructure for ECS phases and statistics
 * 
 * Provides functions to register movement-related infrastructure with Flecs world:
 * - ECS execution phases (Movement, Physics, Sync)
 * - Statistics tracking for movement components
 * 
 * Note: Actual movement computation is handled by GPU compute shaders.
 */
namespace MovementSystem {
    /**
     * @brief Register movement infrastructure with the Flecs world
     * @param world The Flecs world instance
     * @param gpuManager Unused parameter (kept for API compatibility)
     */
    void registerSystems(flecs::world& world, GPUEntityManager* gpuManager = nullptr);
    
    /**
     * @brief Setup movement execution phases
     * @param world The Flecs world instance
     */
    void setupMovementPhases(flecs::world& world);
    
    /**
     * @brief Setup efficient stats observers for entity counting
     * @param world The Flecs world instance
     */
    void setupStatsObservers(flecs::world& world);
    
    /**
     * @brief Reset movement pattern timing to initial state
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