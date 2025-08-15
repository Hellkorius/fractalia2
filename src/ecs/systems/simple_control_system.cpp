#include "simple_control_system.h"
#include "input_system.h"
#include "systems_common.h"
#include "../../graphicstests.h"
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
        std::cout << "T: Run graphics buffer overflow tests" << std::endl;
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
                else if (keyboard->isKeyPressed(SDL_SCANCODE_4)) {
                    controlState->currentMovementType = 4;
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
                
                // Graphics tests with T key
                if (keyboard->isKeyPressed(SDL_SCANCODE_T)) {
                    controlState->requestGraphicsTests = true;
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
        
        // RAII guard ensures flags are reset even if we return early
        ControlStateGuard guard(controlState);
        
        // Handle swarm creation with safety limits
        if (controlState->requestSwarmCreation) {
            auto* gpuManager = renderer.getGPUEntityManager();
            if (!gpuManager) {
                DEBUG_LOG("Error: GPU Entity Manager is null");
                return;
            }
            
            uint32_t currentCount = gpuManager->getEntityCount();
            uint32_t maxEntities = gpuManager->getMaxEntities();
            
            if (currentCount < maxEntities - SystemConstants::MIN_ENTITY_RESERVE_COUNT) {
                DEBUG_LOG("Adding " << SystemConstants::DEFAULT_ENTITY_BATCH_SIZE << " more GPU entities...");
                MovementType currentType = static_cast<MovementType>(controlState->currentMovementType);
                auto newEntities = entityFactory.createSwarmWithType(SystemConstants::DEFAULT_ENTITY_BATCH_SIZE, glm::vec3(0.0f), 2.0f, currentType);
                gpuManager->addEntitiesFromECS(newEntities);
                renderer.uploadPendingGPUEntities();
                DEBUG_LOG("Total GPU entities now: " << gpuManager->getEntityCount());
            } else {
                DEBUG_LOG("Cannot add more entities - limit reached (" << currentCount << "/" << maxEntities << ")");
            }
        }
        
        // Handle single entity creation
        if (controlState->requestEntityCreation) {
            auto* gpuManager = renderer.getGPUEntityManager();
            if (!gpuManager) {
                DEBUG_LOG("Error: GPU Entity Manager is null");
                return;
            }
            
            DEBUG_LOG("Mouse click at world: (" << controlState->entityCreationPos.x << ", " << controlState->entityCreationPos.y << ")");
            MovementType currentType = static_cast<MovementType>(controlState->currentMovementType);
            auto mouseEntity = entityFactory.createMovingEntityWithType(
                glm::vec3(controlState->entityCreationPos.x, controlState->entityCreationPos.y, 0.0f),
                currentType
            );
            if (mouseEntity.is_valid()) {
                std::vector<Entity> entityVec = {mouseEntity};
                gpuManager->addEntitiesFromECS(entityVec);
                renderer.uploadPendingGPUEntities();
                DEBUG_LOG("Created GPU entity with movement pattern");
            }
        }
        
        // Handle movement commands
        if (controlState->currentMovementType != controlState->lastProcessedMovementType) {
            DEBUG_LOG("Movement type command: " << controlState->currentMovementType);
            
            if (renderer.getMovementCommandProcessor()) {
                MovementCommand cmd;
                switch (controlState->currentMovementType) {
                    case SystemConstants::MOVEMENT_TYPE_PETAL: cmd.targetType = MovementCommand::Type::Petal; break;
                    case SystemConstants::MOVEMENT_TYPE_ORBIT: cmd.targetType = MovementCommand::Type::Orbit; break;
                    case SystemConstants::MOVEMENT_TYPE_WAVE: cmd.targetType = MovementCommand::Type::Wave; break;
                    case SystemConstants::MOVEMENT_TYPE_TRIANGLE: cmd.targetType = MovementCommand::Type::TriangleFormation; break;
                    case SystemConstants::MOVEMENT_TYPE_RANDOM_STEP: cmd.targetType = MovementCommand::Type::RandomStep; break;
                }
                cmd.angelMode = controlState->angelModeEnabled;
                cmd.timestamp = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                
                renderer.getMovementCommandProcessor()->getCommandQueue().enqueue(cmd);
            }
            controlState->lastProcessedMovementType = controlState->currentMovementType;
        }
        
        // Handle performance stats request
        if (controlState->requestPerformanceStats) {
            auto* gpuManager = renderer.getGPUEntityManager();
            if (!gpuManager) {
                DEBUG_LOG("Error: GPU Entity Manager is null");
                return;
            }
            
            uint32_t gpuEntityCount = gpuManager->getEntityCount();
            float avgFrameTime = Profiler::getInstance().getFrameTime();
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            // Simple entity count from Flecs directly
            size_t activeEntities = static_cast<size_t>(world.count<Transform>());
            
            auto* appState = world.get<ApplicationState>();
            
            std::cout << "=== Performance Stats ===" << std::endl;
            std::cout << "Frame: " << (appState ? appState->frameCount : 0) << std::endl;
            std::cout << "FPS: " << fps << " (" << avgFrameTime << "ms avg)" << std::endl;
            std::cout << "CPU Entities: " << activeEntities << std::endl;
            std::cout << "GPU Entities: " << gpuEntityCount << "/" << gpuManager->getMaxEntities() << std::endl;
            std::cout << "=========================" << std::endl;
        }
        
        // Handle system scheduler stats request
        if (controlState->requestSystemSchedulerStats) {
            std::cout << "Simple Flecs systems - no complex scheduling" << std::endl;
        }
        
        // Handle graphics tests request
        if (controlState->requestGraphicsTests) {
            GraphicsTests::runAllTests(&renderer);
        }
        
        // Note: Flags are automatically reset by ControlStateGuard destructor
    }
    
}