#pragma once

#include "../component.hpp"
#include <flecs.h>

// Forward declarations
class VulkanRenderer;
class EntityFactory;

// Simple Flecs control system - just handles input, delegates GPU work to renderer
namespace SimpleControlSystem {
    
    // Simple state singleton with GPU operation capabilities
    struct ControlState {
        int currentMovementType = 0;  // 0=Petal, 1=Orbit, 2=Wave, 3=Triangle
        bool angelModeEnabled = false;
        bool requestEntityCreation = false;
        bool requestSwarmCreation = false;
        bool requestPerformanceStats = false;
        bool requestSystemSchedulerStats = false;
        glm::vec2 entityCreationPos{0.0f, 0.0f};
        
        // Reset request flags after processing
        void resetFlags() {
            requestEntityCreation = false;
            requestSwarmCreation = false;
            requestPerformanceStats = false;
            requestSystemSchedulerStats = false;
        }
    };
    
    // Initialize simple control system
    void initialize(flecs::world& world);
    
    // Initialize with phase for integration with SystemScheduler
    void initialize(flecs::world& world, flecs::entity phase);
    
    // Process control actions (called from main loop with renderer/factory references)
    void processControlActions(flecs::world& world, VulkanRenderer& renderer, EntityFactory& entityFactory);
    
}