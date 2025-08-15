#pragma once

#include "../component.h"
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
        bool requestGraphicsTests = false;  // New flag for graphics buffer overflow tests
        glm::vec2 entityCreationPos{0.0f, 0.0f};
        int lastProcessedMovementType = -1;  // Track movement type changes
        
        // Reset request flags after processing
        void resetFlags() {
            requestEntityCreation = false;
            requestSwarmCreation = false;
            requestPerformanceStats = false;
            requestSystemSchedulerStats = false;
            requestGraphicsTests = false;
        }
    };
    
    // RAII helper for automatic control state reset
    class ControlStateGuard {
    private:
        ControlState* state;
    public:
        explicit ControlStateGuard(ControlState* controlState) : state(controlState) {}
        ~ControlStateGuard() {
            if (state) {
                state->resetFlags();
            }
        }
        
        // Non-copyable
        ControlStateGuard(const ControlStateGuard&) = delete;
        ControlStateGuard& operator=(const ControlStateGuard&) = delete;
    };
    
    // Initialize simple control system
    void initialize(flecs::world& world);
    
    // Initialize with phase for integration with SystemScheduler
    void initialize(flecs::world& world, flecs::entity phase);
    
    // Process control actions (called from main loop with renderer/factory references)
    void processControlActions(flecs::world& world, VulkanRenderer& renderer, EntityFactory& entityFactory);
    
}