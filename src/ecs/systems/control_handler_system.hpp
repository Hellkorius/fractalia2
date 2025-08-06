#pragma once

#include "../component.hpp"
#include "../world.hpp"
#include "../profiler.hpp"
#include <flecs.h>
#include <SDL3/SDL.h>

// Control handler system for processing game controls and interactions
namespace ControlHandler {
    
    // Initialize control handler - call this after world and input setup
    void initialize(World& world);
    
    // Process all control inputs - call this each frame after input processing
    void processControls(World& world, bool& running);
    
    // Individual control handler functions
    void handleApplicationControls(World& world, bool& running);
    void handleEntityCreation(World& world);
    void handlePatternSwitching(World& world);
    void handlePerformanceControls(World& world);
    
    // Helper function to switch all entities to a specific movement pattern
    void switchAllEntitiesToPattern(flecs::world& world, MovementType pattern, const std::string& patternName);
}