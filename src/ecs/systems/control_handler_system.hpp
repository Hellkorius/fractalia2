#pragma once

#include "../component.hpp"
#include "../world.hpp"
#include "../profiler.hpp"
#include <flecs.h>
#include <SDL3/SDL.h>

// Forward declaration
class VulkanRenderer;

// Control handler system for processing game controls and interactions
namespace ControlHandler {
    
    // Global movement type state
    extern int g_currentMovementType; // 0=Petal, 1=Orbit, 2=Wave
    
    // Initialize control handler - call this after world and input setup
    void initialize(World& world);
    
    // Process all control inputs - call this each frame after input processing  
    void processControls(World& world, bool& running, VulkanRenderer* renderer = nullptr);
    
    // Individual control handler functions
    void handleApplicationControls(World& world, bool& running);
    void handleEntityCreation(World& world, VulkanRenderer* renderer = nullptr);
    void handlePerformanceControls(World& world, VulkanRenderer* renderer = nullptr);
    void handleMovementTypeControls(World& world, VulkanRenderer* renderer = nullptr);
}