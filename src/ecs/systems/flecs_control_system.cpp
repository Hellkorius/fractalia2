#include "flecs_control_system.hpp"
#include "input_system.hpp"
#include "../../vulkan_renderer.h"
#include "../gpu_entity_manager.h"
#include "../movement_command_system.hpp"
#include "../entity_factory.hpp"
#include <iostream>
#include <chrono>
#include <SDL3/SDL.h>

namespace FlecsControlSystem {
    
    // Global state singleton - stored as Flecs singleton
    static VulkanRenderer* s_renderer = nullptr;
    static EntityFactory* s_entityFactory = nullptr;
    
    void initialize(flecs::world& world, VulkanRenderer* renderer, EntityFactory* entityFactory) {
        s_renderer = renderer;
        s_entityFactory = entityFactory;
        
        // Create singleton components for state management
        world.set<ApplicationState>({});
        world.set<MovementState>({});
        world.set<GPUEntitySync>({});
        
        std::cout << "\n=== Flecs GPU Compute Movement Demo Controls ===" << std::endl;
        std::cout << "ESC: Exit" << std::endl;
        std::cout << "P: Print detailed performance report" << std::endl;
        std::cout << "+/=: Add 1000 more GPU entities" << std::endl;
        std::cout << "-: Show current GPU performance stats" << std::endl;
        std::cout << "Left Click: Create GPU entity with movement at mouse position" << std::endl;
        std::cout << "0/1/2/3: Switch movement pattern (0=Petal, 1=Orbit, 2=Wave, 3=Triangle)" << std::endl;
        std::cout << "CAPS LOCK: Toggle Angel Mode (epic transition effect)" << std::endl;
        std::cout << "\nCamera Controls:" << std::endl;
        std::cout << "WASD: Move camera" << std::endl;
        std::cout << "Q/E: Move camera up/down" << std::endl;
        std::cout << "Mouse Wheel: Zoom in/out" << std::endl;
        std::cout << "R/T: Rotate camera" << std::endl;
        std::cout << "Shift: Speed boost | Ctrl: Precision mode" << std::endl;
        std::cout << "Space: Reset camera to origin" << std::endl;
        std::cout << "C: Print camera info" << std::endl;
        std::cout << "\nGPU Compute Movement:" << std::endl;
        std::cout << "• All movement computed on GPU via compute shader" << std::endl;
        std::cout << "• Petal, orbit, and wave patterns supported" << std::endl;
        std::cout << "===============================================\n" << std::endl;
        
        // Register Flecs systems in proper phases using lambda systems
        world.system("ApplicationControlSystem")
            .kind(flecs::OnUpdate)
            .run([](flecs::iter& it) {
                auto* appState = it.world().get_mut<ApplicationState>();
                auto inputEntity = it.world().lookup("InputManager");
                if (appState && inputEntity.is_valid()) {
                    auto* keyboard = inputEntity.get<KeyboardInput>();
                    if (keyboard && keyboard->isKeyPressed(SDL_SCANCODE_ESCAPE)) {
                        appState->requestQuit = true;
                        appState->running = false;
                        std::cout << "Application quit requested" << std::endl;
                    }
                    appState->frameCount++;
                }
            });
            
        world.system("EntityCreationSystem")
            .kind(flecs::OnUpdate)
            .run([](flecs::iter& it) {
                auto* appState = it.world().get<ApplicationState>();
                auto inputEntity = it.world().lookup("InputManager");
                if (appState && inputEntity.is_valid()) {
                    auto* mouse = inputEntity.get<MouseInput>();
                    auto* keyboard = inputEntity.get<KeyboardInput>();
                    if (mouse && keyboard) {
                        // Handle entity creation logic here
                        if (keyboard->isKeyPressed(SDL_SCANCODE_EQUALS) || 
                            keyboard->isKeyPressed(SDL_SCANCODE_KP_PLUS)) {
                            std::cout << "Adding 1000 more GPU entities..." << std::endl;
                            
                            if (s_renderer && s_renderer->getGPUEntityManager() && s_entityFactory) {
                                auto* movementState = it.world().get_mut<MovementState>();
                                MovementType currentType = static_cast<MovementType>(movementState->currentMovementType);
                                
                                auto newEntities = s_entityFactory->createSwarmWithType(1000, glm::vec3(0.0f), 2.0f, currentType);
                                
                                // Direct upload - observers don't work from within system iterations
                                try {
                                    s_renderer->getGPUEntityManager()->addEntitiesFromECS(newEntities);
                                    s_renderer->uploadPendingGPUEntities();
                                    std::cout << "Added " << newEntities.size() << " entities (direct upload)" << std::endl;
                                    std::cout << "Total GPU entities now: " << s_renderer->getGPUEntityManager()->getEntityCount() << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "Error during GPU upload: " << e.what() << std::endl;
                                } catch (...) {
                                    std::cerr << "Unknown error during GPU upload" << std::endl;
                                }
                            }
                        }
                        
                        if (mouse->isButtonPressed(0)) {
                            std::cout << "Mouse click at world: (" << mouse->worldPosition.x << ", " << mouse->worldPosition.y << ")" << std::endl;
                            
                            if (s_renderer && s_renderer->getGPUEntityManager() && s_entityFactory) {
                                auto* movementState = it.world().get_mut<MovementState>();
                                MovementType currentType = static_cast<MovementType>(movementState->currentMovementType);
                                
                                auto mouseEntity = s_entityFactory->createMovingEntityWithType(
                                    glm::vec3(mouse->worldPosition.x, mouse->worldPosition.y, 0.0f),
                                    currentType
                                );
                                
                                if (mouseEntity.is_valid()) {
                                    // Direct upload - observers don't work from within system iterations
                                    try {
                                        std::vector<Entity> entityVec = {mouseEntity};
                                        s_renderer->getGPUEntityManager()->addEntitiesFromECS(entityVec);
                                        s_renderer->uploadPendingGPUEntities();
                                        std::cout << "Created GPU entity with movement pattern (direct upload)" << std::endl;
                                    } catch (const std::exception& e) {
                                        std::cerr << "Error during mouse entity GPU upload: " << e.what() << std::endl;
                                    } catch (...) {
                                        std::cerr << "Unknown error during mouse entity GPU upload" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            });
            
        world.system("PerformanceControlSystem")
            .kind(flecs::OnUpdate)
            .run([](flecs::iter& it) {
                auto inputEntity = it.world().lookup("InputManager");
                if (inputEntity.is_valid()) {
                    auto* keyboard = inputEntity.get<KeyboardInput>();
                    if (keyboard) {
                        if (keyboard->isKeyPressed(SDL_SCANCODE_P)) {
                            std::cout << "Performance report requested" << std::endl;
                        }
                        if (keyboard->isKeyPressed(SDL_SCANCODE_MINUS) || 
                            keyboard->isKeyPressed(SDL_SCANCODE_KP_MINUS)) {
                            auto* appState = it.world().get<ApplicationState>();
                            uint32_t gpuEntityCount = 0;
                            if (s_renderer && s_renderer->getGPUEntityManager()) {
                                gpuEntityCount = s_renderer->getGPUEntityManager()->getEntityCount();
                            }
                            std::cout << "Current Stats - Frame: " << (appState ? appState->frameCount : 0)
                                      << ", GPU Entities: " << gpuEntityCount << std::endl;
                        }
                    }
                }
            });
            
        world.system("MovementControlSystem")
            .kind(flecs::OnUpdate)
            .run([](flecs::iter& it) {
                auto inputEntity = it.world().lookup("InputManager");
                auto* movementState = it.world().get_mut<MovementState>();
                if (inputEntity.is_valid() && movementState) {
                    auto* keyboard = inputEntity.get<KeyboardInput>();
                    if (keyboard) {
                        // Handle Angel Mode toggle
                        if (keyboard->isKeyPressed(SDL_SCANCODE_CAPSLOCK)) {
                            movementState->angelModeEnabled = !movementState->angelModeEnabled;
                            std::cout << "Angel Mode " << (movementState->angelModeEnabled ? "ENABLED" : "DISABLED") << std::endl;
                        }
                        
                        // Handle movement type switching
                        bool movementChanged = false;
                        MovementCommand::Type newType = MovementCommand::Type::Petal;
                        
                        if (keyboard->isKeyPressed(SDL_SCANCODE_0)) {
                            movementState->currentMovementType = 0;
                            newType = MovementCommand::Type::Petal;
                            movementChanged = true;
                            std::cout << "Movement type command: PETAL (0)" << std::endl;
                        }
                        else if (keyboard->isKeyPressed(SDL_SCANCODE_1)) {
                            movementState->currentMovementType = 1;
                            newType = MovementCommand::Type::Orbit;
                            movementChanged = true;
                            std::cout << "Movement type command: ORBIT (1)" << std::endl;
                        }
                        else if (keyboard->isKeyPressed(SDL_SCANCODE_2)) {
                            movementState->currentMovementType = 2;
                            newType = MovementCommand::Type::Wave;
                            movementChanged = true;
                            std::cout << "Movement type command: WAVE (2)" << std::endl;
                        }
                        else if (keyboard->isKeyPressed(SDL_SCANCODE_3)) {
                            movementState->currentMovementType = 3;
                            newType = MovementCommand::Type::TriangleFormation;
                            movementChanged = true;
                            std::cout << "Movement type command: TRIANGLE FORMATION (3)" << std::endl;
                        }
                        
                        // Enqueue movement command
                        if (movementChanged && s_renderer && s_renderer->getMovementCommandProcessor()) {
                            MovementCommand cmd;
                            cmd.targetType = newType;
                            cmd.angelMode = movementState->angelModeEnabled;
                            cmd.timestamp = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                            
                            s_renderer->getMovementCommandProcessor()->getCommandQueue().enqueue(cmd);
                        }
                    }
                }
            });
        
        // Note: Observer system doesn't work reliably from within Flecs system iterations
        // Using direct upload approach for runtime entity creation
            
        std::cout << "Flecs Control Systems initialized successfully!" << std::endl;
    }
    
    void onGPUEntityCreated(flecs::entity e, const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
        std::cout << "Observer triggered: Entity " << e.id() << " marked for GPU upload" << std::endl;
        
        // Mark entity for GPU upload
        e.add<GPUUploadPending>();
        
        // Update GPU sync singleton to indicate upload needed
        auto* gpuSync = e.world().get_mut<GPUEntitySync>();
        if (gpuSync) {
            gpuSync->needsUpload = true;
            gpuSync->pendingCount++;
            std::cout << "GPU sync updated: pendingCount=" << gpuSync->pendingCount << std::endl;
        } else {
            std::cout << "ERROR: GPUEntitySync singleton not found!" << std::endl;
        }
    }
    
    MovementState* getMovementState(flecs::entity e) {
        return e.world().get_mut<MovementState>();
    }
}