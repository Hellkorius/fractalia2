#include "simple_control_system.hpp"
#include "input_system.hpp"
#include <iostream>
#include <SDL3/SDL.h>

namespace SimpleControlSystem {
    
    void initialize(flecs::world& world) {
        initialize(world, flecs::entity::null()); // Call with null phase for backward compatibility
    }
    
    void initialize(flecs::world& world, flecs::entity phase) {
        // Create control state singleton
        world.set<ControlState>({});
        
        // Create ApplicationState if it doesn't exist
        if (!world.has<ApplicationState>()) {
            world.set<ApplicationState>({});
        }
        
        std::cout << "\n=== Flecs GPU Compute Movement Demo Controls ===" << std::endl;
        std::cout << "ESC: Exit" << std::endl;
        std::cout << "P: Print detailed performance report" << std::endl;
        std::cout << "I: Print system scheduler performance report" << std::endl;
        std::cout << "+/=: Add 1000 more GPU entities" << std::endl;
        std::cout << "-: Show current GPU performance stats" << std::endl;
        std::cout << "Left Click: Create GPU entity with movement at mouse position" << std::endl;
        std::cout << "0/1/2/3: Switch movement pattern (0=Petal, 1=Orbit, 2=Wave, 3=Triangle)" << std::endl;
        std::cout << "CAPS LOCK: Toggle Angel Mode (epic transition effect)" << std::endl;
        std::cout << "===============================================\n" << std::endl;
        
        // Simple input handling system - register with appropriate phase
        auto controlSystem = world.system("ControlInputSystem")
            .run([](flecs::iter& it) {
                auto* controlState = it.world().get_mut<ControlState>();
                auto* appState = it.world().get_mut<ApplicationState>();
                auto inputEntity = it.world().lookup("InputManager");
                
                if (!controlState || !appState || !inputEntity.is_valid()) {
                    return;
                }
                
                auto* keyboard = inputEntity.get<KeyboardInput>();
                auto* mouse = inputEntity.get<MouseInput>();
                
                if (!keyboard || !mouse) {
                    return;
                }
                
                // Application controls
                if (keyboard->isKeyPressed(SDL_SCANCODE_ESCAPE)) {
                    appState->requestQuit = true;
                    appState->running = false;
                }
                
                // Entity creation controls - use frame-based detection to prevent flooding
                if (keyboard->isKeyPressed(SDL_SCANCODE_EQUALS) || 
                    keyboard->isKeyPressed(SDL_SCANCODE_KP_PLUS)) {
                    controlState->requestSwarmCreation = true;
                }
                
                if (mouse->isButtonPressed(0)) {
                    controlState->requestEntityCreation = true;
                    controlState->entityCreationPos = mouse->worldPosition;
                }
                
                // Movement type switching - frame-based to prevent spam
                if (keyboard->isKeyPressed(SDL_SCANCODE_0)) {
                    controlState->currentMovementType = 0;
                }
                else if (keyboard->isKeyPressed(SDL_SCANCODE_1)) {
                    controlState->currentMovementType = 1;
                }
                else if (keyboard->isKeyPressed(SDL_SCANCODE_2)) {
                    controlState->currentMovementType = 2;
                }
                else if (keyboard->isKeyPressed(SDL_SCANCODE_3)) {
                    controlState->currentMovementType = 3;
                }
                
                // Angel mode toggle - frame-based
                if (keyboard->isKeyPressed(SDL_SCANCODE_CAPSLOCK)) {
                    controlState->angelModeEnabled = !controlState->angelModeEnabled;
                    std::cout << "Angel Mode " << (controlState->angelModeEnabled ? "ENABLED" : "DISABLED") << std::endl;
                }
                
                // Performance stats - frame-based
                if (keyboard->isKeyPressed(SDL_SCANCODE_P)) {
                    controlState->requestPerformanceStats = true;
                }
                
                // System scheduler performance report
                if (keyboard->isKeyPressed(SDL_SCANCODE_I)) {
                    controlState->requestSystemSchedulerStats = true;
                }
                
                // Quick stats with minus key
                if (keyboard->isKeyPressed(SDL_SCANCODE_MINUS) || 
                    keyboard->isKeyPressed(SDL_SCANCODE_KP_MINUS)) {
                    controlState->requestPerformanceStats = true;
                }
                
                appState->frameCount++;
            });
        
        // Register with phase if provided
        if (phase.is_valid()) {
            controlSystem.child_of(phase);
            std::cout << "Simple Control System initialized with phase!" << std::endl;
        } else {
            // Backward compatibility - system runs in default phase
            std::cout << "Simple Control System initialized!" << std::endl;
        }
    }
    
}