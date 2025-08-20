#include "movement_system.h"
#include <chrono>
#include <cmath>

namespace MovementSystem {
    // Stats tracking
    static MovementStats stats_;
    
    // System callback functions
    static void movementPatternUpdateSystem(flecs::entity e, Transform& transform, MovementPattern& pattern) {
        float deltaTime = e.world().delta_time();
        
        pattern.currentTime += deltaTime;
        
        if (pattern.movementType == MovementType::RandomWalk) {
            float phase = pattern.phase + pattern.currentTime * pattern.frequency;
            
            // Use sine waves with phase offset for smooth random-like movement
            glm::vec3 offset;
            offset.x = sin(phase) * pattern.amplitude;
            offset.y = cos(phase * 1.3f) * pattern.amplitude * 0.7f;
            offset.z = sin(phase * 0.8f) * pattern.amplitude * 0.5f;
            
            transform.position = pattern.center + offset;
        }
        
        // Stats are counted globally, not per entity
    }
    
    static void physicsUpdateSystem(flecs::entity e, Transform& transform, Velocity& velocity) {
        float deltaTime = e.world().delta_time();
        
        // Apply velocity to position
        transform.position += velocity.linear * deltaTime;
        
        // Apply angular velocity to rotation
        glm::vec3 angularDelta = velocity.angular * deltaTime;
        glm::quat deltaRotation = glm::quat(angularDelta);
        transform.rotation = deltaRotation * transform.rotation;
        
        // Simple damping
        velocity.linear *= 0.98f;
        velocity.angular *= 0.95f;
    }
    
    static void movementSyncSystem(flecs::entity e, Transform& transform, Renderable& renderable, MovementPattern& pattern) {
        // Update model matrix in renderable component
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), transform.position);
        glm::mat4 rotationMatrix = glm::mat4_cast(transform.rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);
        
        renderable.modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    }
    
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
    
    void registerSystems(flecs::world& world, GPUEntityManager* gpuManager) {
        // Reset stats on registration
        resetStats();
        
        // Setup phases first
        setupMovementPhases(world);
        
        auto movementPhase = world.entity("MovementPhase");
        auto physicsPhase = world.entity("PhysicsPhase");
        auto movementSyncPhase = world.entity("MovementSyncPhase");
        
        // Register movement pattern update system
        world.system<Transform, MovementPattern>("MovementPatternSystem")
            .kind(movementPhase)
            .each(movementPatternUpdateSystem);
            
        // Register physics update system
        world.system<Transform, Velocity>("PhysicsSystem")
            .kind(physicsPhase)
            .each(physicsUpdateSystem);
            
        // Register movement synchronization system
        world.system<Transform, Renderable, MovementPattern>("MovementSyncSystem")
            .kind(movementSyncPhase)
            .each(movementSyncSystem);
            
        // Register stats tracking system (runs after all movement systems)
        world.system("MovementStatsSystem")
            .kind(flecs::PostUpdate)
            .run([](flecs::iter& it) {
                // Reset stats each frame
                stats_.entitiesWithMovement = 0;
                stats_.entitiesWithPhysics = 0;
                
                // Count entities with movement patterns
                it.world().query<Transform, MovementPattern>().each([](flecs::entity e, Transform&, MovementPattern&) {
                    stats_.entitiesWithMovement++;
                });
                
                // Count entities with physics
                it.world().query<Transform, Velocity>().each([](flecs::entity e, Transform&, Velocity&) {
                    stats_.entitiesWithPhysics++;
                });
            });
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