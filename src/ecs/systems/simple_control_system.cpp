#include "simple_control_system.hpp"
#include "input_system.hpp"
#include "../../vulkan_renderer.h"
#include "../entity_factory.hpp"
#include "../movement_command_system.hpp"
#include "../profiler.hpp"
#include "../gpu_entity_manager.h"
#include <iostream>
#include <SDL3/SDL.h>
#include <chrono>

// Debug output control
#ifdef NDEBUG
#define DEBUG_LOG(x) do {} while(0)
#else
#define DEBUG_LOG(x) std::cout << x << std::endl
#endif

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
    
    void processControlActions(flecs::world& world, VulkanRenderer& renderer, EntityFactory& entityFactory) {
        auto* controlState = world.get_mut<ControlState>();
        if (!controlState) {
            return;
        }
        
        // Handle swarm creation with safety limits
        if (controlState->requestSwarmCreation) {
            uint32_t currentCount = renderer.getGPUEntityManager()->getEntityCount();
            uint32_t maxEntities = renderer.getGPUEntityManager()->getMaxEntities();
            
            if (currentCount < maxEntities - 1000) {
                DEBUG_LOG("Adding 1000 more GPU entities...");
                MovementType currentType = static_cast<MovementType>(controlState->currentMovementType);
                auto newEntities = entityFactory.createSwarmWithType(1000, glm::vec3(0.0f), 2.0f, currentType);
                renderer.getGPUEntityManager()->addEntitiesFromECS(newEntities);
                renderer.uploadPendingGPUEntities();
                DEBUG_LOG("Total GPU entities now: " << renderer.getGPUEntityManager()->getEntityCount());
            } else {
                DEBUG_LOG("Cannot add more entities - limit reached (" << currentCount << "/" << maxEntities << ")");
            }
        }
        
        // Handle single entity creation
        if (controlState->requestEntityCreation) {
            DEBUG_LOG("Mouse click at world: (" << controlState->entityCreationPos.x << ", " << controlState->entityCreationPos.y << ")");
            MovementType currentType = static_cast<MovementType>(controlState->currentMovementType);
            auto mouseEntity = entityFactory.createMovingEntityWithType(
                glm::vec3(controlState->entityCreationPos.x, controlState->entityCreationPos.y, 0.0f),
                currentType
            );
            if (mouseEntity.is_valid()) {
                std::vector<Entity> entityVec = {mouseEntity};
                renderer.getGPUEntityManager()->addEntitiesFromECS(entityVec);
                renderer.uploadPendingGPUEntities();
                DEBUG_LOG("Created GPU entity with movement pattern");
            }
        }
        
        // Handle movement commands
        static int lastMovementType = -1;
        if (controlState->currentMovementType != lastMovementType) {
            DEBUG_LOG("Movement type command: " << controlState->currentMovementType);
            
            if (renderer.getMovementCommandProcessor()) {
                MovementCommand cmd;
                switch (controlState->currentMovementType) {
                    case 0: cmd.targetType = MovementCommand::Type::Petal; break;
                    case 1: cmd.targetType = MovementCommand::Type::Orbit; break;
                    case 2: cmd.targetType = MovementCommand::Type::Wave; break;
                    case 3: cmd.targetType = MovementCommand::Type::TriangleFormation; break;
                }
                cmd.angelMode = controlState->angelModeEnabled;
                cmd.timestamp = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                
                renderer.getMovementCommandProcessor()->getCommandQueue().enqueue(cmd);
            }
            lastMovementType = controlState->currentMovementType;
        }
        
        // Handle performance stats request
        if (controlState->requestPerformanceStats) {
            uint32_t gpuEntityCount = renderer.getGPUEntityManager()->getEntityCount();
            float avgFrameTime = Profiler::getInstance().getFrameTime();
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            // Simple entity count from Flecs directly
            size_t activeEntities = static_cast<size_t>(world.count<Transform>());
            
            auto* appState = world.get<ApplicationState>();
            
            std::cout << "=== Performance Stats ===" << std::endl;
            std::cout << "Frame: " << (appState ? appState->frameCount : 0) << std::endl;
            std::cout << "FPS: " << fps << " (" << avgFrameTime << "ms avg)" << std::endl;
            std::cout << "CPU Entities: " << activeEntities << std::endl;
            std::cout << "GPU Entities: " << gpuEntityCount << "/" << renderer.getGPUEntityManager()->getMaxEntities() << std::endl;
            std::cout << "=========================" << std::endl;
        }
        
        // Handle system scheduler stats request
        if (controlState->requestSystemSchedulerStats) {
            std::cout << "Simple Flecs systems - no complex scheduling" << std::endl;
        }
        
        // Reset request flags
        controlState->resetFlags();
    }
    
}