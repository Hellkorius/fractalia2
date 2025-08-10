#pragma once

#include "../component.hpp"
#include "../world.hpp"
#include <flecs.h>

// Forward declarations
class VulkanRenderer;
class EntityFactory;

// Flecs-native control system that replaces the manual ControlHandler
namespace FlecsControlSystem {
    
    // Initialize the Flecs control systems and observers
    void initialize(flecs::world& world, VulkanRenderer* renderer, EntityFactory* entityFactory);
    
    // GPU entity creation observer - triggers when new entities need GPU upload
    void onGPUEntityCreated(flecs::entity e, const Transform& transform, const Renderable& renderable, const MovementPattern& pattern);
    
    // Global movement pattern state (replaces ControlHandler globals)
    struct MovementState {
        int currentMovementType = 0;  // 0=Petal, 1=Orbit, 2=Wave, 3=Triangle
        bool angelModeEnabled = false; // CAPS LOCK: Enable "biblically accurate angel" transition
    };
    
    // Get the global movement state
    MovementState* getMovementState(flecs::entity e);
}