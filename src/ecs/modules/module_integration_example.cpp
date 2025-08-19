/**
 * @file module_integration_example.cpp
 * @brief Example demonstrating how to integrate and use the ECS modular system
 * 
 * This file provides a complete example of how to:
 * 1. Initialize the WorldManager
 * 2. Load and configure all modules
 * 3. Set up proper dependencies
 * 4. Execute frame updates
 * 5. Handle module lifecycle
 */

#include "../core/world_manager.h"
#include "input_module.h"
#include "movement_module.h"
#include "rendering_module.h"
#include "../core/service_locator.h"
#include "../../vulkan_renderer.h"
#include "../../ecs/gpu_entity_manager.h"
#include <SDL3/SDL.h>
#include "../camera_component.h"
#include "../component.h"
#include <iostream>
#include <memory>

/**
 * @brief Example ECS modular system integration
 */
class ECSModularSystemExample {
public:
    ECSModularSystemExample() = default;
    ~ECSModularSystemExample() = default;

    bool initialize(SDL_Window* window, VulkanRenderer* renderer) {
        try {
            // 1. Initialize the WorldManager service
            worldManager_ = std::make_shared<WorldManager>();
            ServiceLocator::instance().registerService<WorldManager>(worldManager_);
            
            if (!worldManager_->initialize()) {
                std::cerr << "Failed to initialize WorldManager" << std::endl;
                return false;
            }

            // 2. Load modules in dependency order
            
            // Load Input Module first (no dependencies)
            inputModule_ = worldManager_->loadModule<InputModule>("InputModule", window);
            if (!inputModule_) {
                std::cerr << "Failed to load InputModule" << std::endl;
                return false;
            }

            // Load Movement Module (depends on having entities to move)
            gpuEntityManager_ = renderer->getGPUEntityManager();
            movementModule_ = worldManager_->loadModule<MovementModule>("MovementModule", gpuEntityManager_);
            if (!movementModule_) {
                std::cerr << "Failed to load MovementModule" << std::endl;
                return false;
            }

            // Load Rendering Module last (depends on input and movement)
            renderingModule_ = worldManager_->loadModule<RenderingModule>("RenderingModule", renderer, gpuEntityManager_);
            if (!renderingModule_) {
                std::cerr << "Failed to load RenderingModule" << std::endl;
                return false;
            }

            // 3. Configure module interactions
            setupModuleInteractions();

            // 4. Create some test entities
            createTestEntities();

            // 5. Set up performance monitoring
            worldManager_->enablePerformanceMonitoring(true);
            worldManager_->registerPerformanceCallback([this](float avgFrameTime) {
                handlePerformanceUpdate(avgFrameTime);
            });

            std::cout << "ECS Modular System initialized successfully" << std::endl;
            std::cout << "Loaded modules:" << std::endl;
            std::cout << "  - " << inputModule_->getName() << std::endl;
            std::cout << "  - " << movementModule_->getName() << std::endl;
            std::cout << "  - " << renderingModule_->getName() << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception during initialization: " << e.what() << std::endl;
            return false;
        }
    }

    void shutdown() {
        try {
            if (worldManager_) {
                // Unload modules in reverse order
                worldManager_->unloadModule("RenderingModule");
                worldManager_->unloadModule("MovementModule"); 
                worldManager_->unloadModule("InputModule");
                
                worldManager_->shutdown();
                worldManager_.reset();
            }

            ServiceLocator::instance().unregisterService<WorldManager>();

            std::cout << "ECS Modular System shutdown complete" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Exception during shutdown: " << e.what() << std::endl;
        }
    }

    void updateFrame(float deltaTime) {
        if (!worldManager_) {
            return;
        }

        try {
            // Execute the frame through WorldManager
            // This will call update() on all loaded modules and run ECS systems
            worldManager_->executeFrame(deltaTime);

            // Example of module-specific operations
            handleInputCommands();
            processMovementCommands();
            updateRenderingState();

        } catch (const std::exception& e) {
            std::cerr << "Exception during frame update: " << e.what() << std::endl;
        }
    }

    // Performance and debugging information
    void printStats() const {
        if (!worldManager_) {
            return;
        }

        std::cout << "\n=== ECS Modular System Stats ===" << std::endl;
        std::cout << "Entity Count: " << worldManager_->getEntityCount() << std::endl;
        std::cout << "Average Frame Time: " << worldManager_->getAverageFrameTime() << " ms" << std::endl;
        std::cout << "FPS: " << worldManager_->getFPS() << std::endl;

        if (movementModule_) {
            const auto& movementStats = movementModule_->getStats();
            std::cout << "\nMovement Module Stats:" << std::endl;
            std::cout << "  Entities with Movement: " << movementStats.entitiesWithMovement << std::endl;
            std::cout << "  Commands Processed: " << movementStats.commandsProcessed << std::endl;
            std::cout << "  Commands Enqueued: " << movementStats.commandsEnqueued << std::endl;
            std::cout << "  Last Update Time: " << movementStats.lastUpdateTime << " ms" << std::endl;
        }

        if (renderingModule_) {
            const auto& renderingStats = renderingModule_->getStats();
            std::cout << "\nRendering Module Stats:" << std::endl;
            std::cout << "  Total Entities: " << renderingStats.totalEntities << std::endl;
            std::cout << "  Visible Entities: " << renderingStats.visibleEntities << std::endl;
            std::cout << "  Culled Entities: " << renderingStats.culledEntities << std::endl;
            std::cout << "  LOD Level 0: " << renderingStats.lodLevel0Entities << std::endl;
            std::cout << "  LOD Level 1: " << renderingStats.lodLevel1Entities << std::endl;
            std::cout << "  LOD Level 2: " << renderingStats.lodLevel2Entities << std::endl;
        }
        std::cout << "==============================\n" << std::endl;
    }

private:
    std::shared_ptr<WorldManager> worldManager_;
    std::shared_ptr<InputModule> inputModule_;
    std::shared_ptr<MovementModule> movementModule_;
    std::shared_ptr<RenderingModule> renderingModule_;
    GPUEntityManager* gpuEntityManager_ = nullptr;

    void setupModuleInteractions() {
        // Example: Set up camera entity for rendering module
        auto& world = worldManager_->getWorld();
        
        // Create camera entity
        auto camera = world.entity("MainCamera")
            .add<Transform>()
            .add<Camera>();
            
        if (renderingModule_) {
            renderingModule_->setCameraEntity(camera);
        }

        // Configure rendering state
        if (renderingModule_) {
            RenderingModule::RenderState renderState;
            renderState.cullingEnabled = true;
            renderState.lodEnabled = true;
            renderState.maxRenderableEntities = 80000;
            renderingModule_->setRenderState(renderState);
        }
    }

    void createTestEntities() {
        auto& world = worldManager_->getWorld();

        // Create a few test entities with different components
        for (int i = 0; i < 10; ++i) {
            auto entity = world.entity()
                .add<Transform>()
                .add<Renderable>()
                .add<MovementPattern>()
                .add<CullingData>()
                .add<LODData>();

            // Initialize transform
            auto transform = entity.get_mut<Transform>();
            transform->position = glm::vec3(
                static_cast<float>(i - 5) * 2.0f, 
                0.0f, 
                0.0f
            );

            // Initialize movement pattern
            auto movement = entity.get_mut<MovementPattern>();
            movement->amplitude = 1.0f + static_cast<float>(i) * 0.1f;
            movement->frequency = 0.5f + static_cast<float>(i) * 0.1f;
            movement->center = transform->position;

            // Initialize renderable
            auto renderable = entity.get_mut<Renderable>();
            renderable->color = glm::vec4(
                static_cast<float>(i) / 10.0f,
                1.0f - static_cast<float>(i) / 10.0f,
                0.5f,
                1.0f
            );
        }

        std::cout << "Created " << 10 << " test entities" << std::endl;
    }

    void handleInputCommands() {
        if (!inputModule_) {
            return;
        }

        // Example: Check for quit input
        if (inputModule_->shouldQuit()) {
            std::cout << "Quit requested via input" << std::endl;
        }

        // Example: Handle movement commands from input
        if (inputModule_->isKeyPressed(SDL_SCANCODE_SPACE)) {
            // Enqueue a random walk movement command
            MovementCommand command;
            command.targetType = MovementCommand::Type::RandomWalk;
            command.timestamp = SDL_GetTicks() / 1000.0;

            if (movementModule_) {
                movementModule_->enqueueMovementCommand(command);
            }
        }
    }

    void processMovementCommands() {
        if (!movementModule_) {
            return;
        }

        // Process any pending movement commands
        movementModule_->processMovementCommands();
    }

    void updateRenderingState() {
        if (!renderingModule_) {
            return;
        }

        // Example: Perform culling and LOD updates
        // In a real application, you'd get camera position from camera entity
        glm::vec3 cameraPosition(0.0f, 0.0f, 5.0f);
        glm::mat4 viewMatrix = glm::lookAt(
            cameraPosition,
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        glm::mat4 projMatrix = glm::perspective(
            glm::radians(45.0f),
            16.0f / 9.0f,
            0.1f,
            100.0f
        );

        renderingModule_->performCulling(cameraPosition, viewMatrix, projMatrix);
        renderingModule_->updateLOD(cameraPosition);
        renderingModule_->synchronizeWithGPU();
    }

    void handlePerformanceUpdate(float avgFrameTime) {
        // Example performance monitoring
        static int updateCount = 0;
        updateCount++;

        // Print stats every 60 performance updates (approximately once per second at 60 FPS)
        if (updateCount % 60 == 0) {
            std::cout << "Performance Update - Avg Frame Time: " << avgFrameTime << " ms" << std::endl;
        }
    }
};

/**
 * @brief Example usage function showing complete integration
 */
void demonstrateECSModularSystem(SDL_Window* window, VulkanRenderer* renderer) {
    ECSModularSystemExample example;
    
    if (!example.initialize(window, renderer)) {
        std::cerr << "Failed to initialize ECS modular system example" << std::endl;
        return;
    }

    std::cout << "Running ECS modular system for 5 seconds..." << std::endl;

    // Run for a short time to demonstrate functionality
    const float deltaTime = 1.0f / 60.0f; // 60 FPS
    const int maxFrames = 300; // 5 seconds at 60 FPS

    for (int frame = 0; frame < maxFrames; ++frame) {
        example.updateFrame(deltaTime);
        
        // Print stats every second
        if (frame % 60 == 0) {
            example.printStats();
        }
        
        // Simulate frame timing
        SDL_Delay(16); // ~60 FPS
    }

    example.shutdown();
    std::cout << "ECS modular system demonstration complete" << std::endl;
}