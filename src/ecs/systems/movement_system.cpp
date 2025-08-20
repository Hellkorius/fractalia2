#include "movement_system.h"
#include <chrono>
#include <cmath>

namespace MovementSystem {
    // Stats tracking
    static MovementStats stats_;
    
    // CPU movement system functions removed - movement is GPU-driven via compute shaders
    // GPU reads GPUEntity data and calculates positions directly
    
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
        
        // Setup phases first (still needed for potential future systems)
        setupMovementPhases(world);
        
        // CPU movement systems DISABLED - movement is handled on GPU via compute shaders
        // The GPU compute shader reads GPUEntity data and updates positions directly
        // CPU-side Transform components are only used for initial setup
        
        // NOTE: If you need to re-enable CPU movement for debugging:
        // world.system<Transform, MovementPattern>("MovementPatternSystem")
        //     .kind(movementPhase)
        //     .each(movementPatternUpdateSystem);
        // world.system<Transform, Velocity>("PhysicsSystem")  
        //     .kind(physicsPhase)
        //     .each(physicsUpdateSystem);
        // world.system<Transform, Renderable, MovementPattern>("MovementSyncSystem")
        //     .kind(movementSyncPhase)
        //     .each(movementSyncSystem);
            
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