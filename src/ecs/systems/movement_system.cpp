#include "movement_system.h"
#include <chrono>
#include <cmath>

namespace MovementSystem {
    // Stats tracking
    static MovementStats stats_;
    
    // Movement computation is handled by GPU compute shaders
    // This file only provides ECS phases and statistics tracking
    
    void setupMovementPhases(flecs::world& world) {
        // Create movement update phase that runs after input but before rendering
        auto movementPhase = world.entity("MovementPhase")
            .add(flecs::Phase)
            .depends_on(flecs::OnUpdate);
            
        // Create physics phase that runs after movement
        auto physicsPhase = world.entity("PhysicsPhase")
            .add(flecs::Phase)
            .depends_on(movementPhase);
            
        // Create movement sync phase that runs after physics
        world.entity("MovementSyncPhase")
            .add(flecs::Phase)
            .depends_on(physicsPhase);
    }
    
    void setupStatsObservers(flecs::world& world) {
        // Efficient entity counting using Flecs observers - only updates on add/remove
        world.observer<MovementPattern>("MovementStatsObserver")
            .event(flecs::OnAdd)
            .each([](flecs::entity e, MovementPattern&) {
                stats_.entitiesWithMovement++;
            });
            
        world.observer<MovementPattern>("MovementStatsRemoveObserver")
            .event(flecs::OnRemove)
            .each([](flecs::entity e, MovementPattern&) {
                stats_.entitiesWithMovement--;
            });
            
        world.observer<Velocity>("PhysicsStatsObserver")
            .event(flecs::OnAdd)
            .each([](flecs::entity e, Velocity&) {
                stats_.entitiesWithPhysics++;
            });
            
        world.observer<Velocity>("PhysicsStatsRemoveObserver")
            .event(flecs::OnRemove)
            .each([](flecs::entity e, Velocity&) {
                stats_.entitiesWithPhysics--;
            });
    }
    
    void registerSystems(flecs::world& world, GPUEntityManager* gpuManager) {
        // Reset stats on registration
        resetStats();
        
        // Setup ECS execution phases for component organization
        setupMovementPhases(world);
        
        // Movement computation is handled by GPU compute shaders
        // CPU-side Transform components are only used for initial entity setup
            
        // Register efficient stats tracking system using observers
        setupStatsObservers(world);
    }
    
    void resetAllMovementPatterns(flecs::world& world) {
        world.query<MovementPattern>().each([](flecs::entity e, MovementPattern& pattern) {
            pattern.currentTime = 0.0f;
            pattern.phase = 0.0f;
            // Keep other parameters like amplitude, frequency, center intact
        });
    }
    
    const MovementStats& getStats() {
        return stats_;
    }
    
    void resetStats() {
        stats_ = {};
    }
}