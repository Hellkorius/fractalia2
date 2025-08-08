#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <chrono>
#include "vulkan_renderer.h"
#include "PolygonFactory.h"
#include "ecs/world.hpp"
#include "ecs/systems/render_system.cpp"
#include "ecs/systems/physics_system.hpp"
#include "ecs/systems/movement_system.hpp"
#include "ecs/systems/input_system.hpp"
#include "ecs/systems/camera_system.hpp"
#include "ecs/systems/control_handler_system.hpp"
#include "ecs/camera_component.hpp"
#include "ecs/profiler.hpp"
#include "ecs/gpu_entity_manager.h"

int main(int argc, char* argv[]) {
    constexpr int TARGET_FPS = 60;
    constexpr float TARGET_FRAME_TIME = 1000.0f / TARGET_FPS; // 16.67ms
    
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }
    

    // Check Vulkan support
    uint32_t extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensions || extensionCount == 0) {
        std::cerr << "Vulkan is not supported or no Vulkan extensions available" << std::endl;
        std::cerr << "Make sure Vulkan drivers are installed" << std::endl;
        SDL_Quit();
        return -1;
    }
    

    SDL_Window* window = SDL_CreateWindow(
        "Fractalia2 - SDL3 + Vulkan + Flecs",
        800, 600,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    

    VulkanRenderer renderer;
    if (!renderer.initialize(window)) {
        std::cerr << "Failed to initialize Vulkan renderer" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    World world;

    // NOTE: Movement systems disabled - now handled by GPU compute shader
    // world.getFlecsWorld().system<Transform, MovementPattern>()
    //     .each(movement_system);
    //     
    // world.getFlecsWorld().system<Transform, Velocity>()
    //     .each(velocity_movement_system);
        
    world.getFlecsWorld().system<Lifetime>()
        .each(lifetime_system);
    
    // Input processing system
    world.getFlecsWorld().system<InputState, KeyboardInput, MouseInput, InputEvents>()
        .each(input_processing_system);
    
    // Camera systems - process camera input and update matrices
    world.getFlecsWorld().system<Camera>()
        .each([](flecs::entity e, Camera& camera) {
            camera_control_system(e, camera, e.world().delta_time());
        });
        
    world.getFlecsWorld().system<Camera>()
        .each(camera_matrix_system);
    
    // Create input singleton entity and set window reference for accurate screen coordinates
    InputManager::createInputEntity(world.getFlecsWorld());
    InputManager::setWindow(window);
    
    // Create main camera entity
    CameraManager::createMainCamera(world.getFlecsWorld());
    
    // Set world reference in renderer for camera integration
    renderer.setWorld(&world.getFlecsWorld());
    
    // Initialize render system
    RenderSystem renderSystem(&renderer, world.getFlecsWorld());
    
    // Configure profiler for 60 FPS
    Profiler::getInstance().setTargetFrameTime(TARGET_FRAME_TIME);

    // GPU stress test configuration - scale up to 10k for initial test
    constexpr size_t ENTITY_COUNT = 5; // GPU can handle much more
    
    std::cout << "Creating " << ENTITY_COUNT << " GPU entities for stress testing..." << std::endl;
    
    // Create a swarm of entities for stress testing - these will be uploaded to GPU
    auto swarmEntities = world.getEntityFactory().createSwarm(
        ENTITY_COUNT,
        glm::vec3(0.0f, 0.0f, 0.0f), // center
        2.0f  // larger radius for more spread
    );
    
    // Upload all entities to GPU immediately
    renderer.getGPUEntityManager()->addEntitiesFromECS(swarmEntities);
    
    std::cout << "Created " << swarmEntities.size() << " GPU entities!" << std::endl;
    
    // Initialize control handler system
    ControlHandler::initialize(world);

    bool running = true;
    int frameCount = 0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStartTime - lastFrameTime).count();
        lastFrameTime = frameStartTime;
        
        // Process SDL events through the input system
        InputManager::processSDLEvents(world.getFlecsWorld());
        
        // Handle all controls through the control handler system
        ControlHandler::processControls(world, running, &renderer);
        
        // Handle window resize for camera aspect ratio
        auto inputEntity = world.getFlecsWorld().lookup("InputManager");
        if (inputEntity.is_valid()) {
            auto* events = inputEntity.get<InputEvents>();
            if (events) {
                for (size_t i = 0; i < events->eventCount; i++) {
                    const auto& event = events->events[i];
                    if (event.type == InputEvents::Event::WINDOW_RESIZE) {
                        renderer.updateAspectRatio(event.windowResizeEvent.width, event.windowResizeEvent.height);
                        std::cout << "Window resized to " << event.windowResizeEvent.width << "x" << event.windowResizeEvent.height << std::endl;
                    }
                }
            }
        }

        // Profile the main update loop
        PROFILE_BEGIN_FRAME();
        
        // Set deltaTime for GPU compute shader
        renderer.setDeltaTime(deltaTime);
        
        {
            PROFILE_SCOPE("ECS Update");
            // Only run non-movement systems (input, camera, etc.)
            world.progress(deltaTime);
        }
        
        // Clear input frame states AFTER all systems have processed input
        {
            PROFILE_SCOPE("Input Cleanup");
            auto inputEntity = world.getFlecsWorld().lookup("InputManager");
            if (inputEntity.is_valid()) {
                auto* keyboard = inputEntity.get_mut<KeyboardInput>();
                auto* mouse = inputEntity.get_mut<MouseInput>();
                auto* events = inputEntity.get_mut<InputEvents>();
                if (keyboard) keyboard->clearFrameStates();
                if (mouse) mouse->clearFrameStates();
                if (events) events->clear();
            }
        }

        // Only update render system when there are ECS changes (entity creation/destruction)
        // For GPU entities, the compute pipeline handles all movement/rendering
        static uint32_t lastEntityCount = 0;
        uint32_t currentEntityCount = static_cast<uint32_t>(world.getFlecsWorld().count<Renderable>());
        
        if (currentEntityCount != lastEntityCount) {
            PROFILE_SCOPE("Render System - ECS Changes");
            renderSystem.update();
            lastEntityCount = currentEntityCount;
            std::cout << "ECS entities updated: " << currentEntityCount << std::endl;
        }
        

        {
            PROFILE_SCOPE("Vulkan Rendering");
            renderer.drawFrame();
        }

        frameCount++;
        PROFILE_END_FRAME();
        
        // Show periodic performance info
        if (frameCount % 300 == 0) { // Every 5 seconds at 60fps
            float avgFrameTime = Profiler::getInstance().getFrameTime();
            auto worldStats = world.getStats();
            
            // Update profiler with current memory usage
            Profiler::getInstance().updateMemoryUsage(worldStats.memoryStats.totalAllocated);
            
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            std::cout << "Frame " << frameCount 
                      << ": Avg " << avgFrameTime << "ms"
                      << " (" << fps << " FPS)"
                      << " | Memory: " << (worldStats.memoryStats.totalAllocated / 1024) << "KB"
                      << std::endl;
        }
        
        // Cap framerate at TARGET_FPS
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        float frameTimeSeconds = std::chrono::duration<float>(frameEndTime - frameStartTime).count();
        float frameTimeMs = frameTimeSeconds * 1000.0f;
        
        if (frameTimeMs < TARGET_FRAME_TIME) {
            int delayMs = static_cast<int>(TARGET_FRAME_TIME - frameTimeMs);
            if (delayMs > 0) {
                SDL_Delay(delayMs);
            }
        }
    }


    renderer.cleanup();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}