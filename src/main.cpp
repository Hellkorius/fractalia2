#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <chrono>
#include <thread>
#include "vulkan_renderer.h"
#include "PolygonFactory.h"
#include "ecs/world.hpp"
#include "ecs/system_registry.hpp"
#include "ecs/systems/lifetime_system.hpp"
#include "ecs/systems/input_system.hpp"
#include "ecs/systems/camera_system.hpp"
#include "ecs/systems/control_handler_system.hpp"
#include "ecs/camera_component.hpp"
#include "ecs/profiler.hpp"
#include "ecs/gpu_entity_manager.h"

int main(int argc, char* argv[]) {
    constexpr int TARGET_FPS = 60;
    constexpr float TARGET_FRAME_TIME = 1000.0f / TARGET_FPS; // 16.67ms
    
    // For silky smooth rendering, we'll rely more on Vulkan present modes (MAILBOX/FIFO)
    // and use a more relaxed frame cap to prevent conflicts
    
    
    // Set SDL vsync hint to 0 for safety (ignored with pure Vulkan, but good practice)
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    
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

    // Initialize system scheduler with Flecs-native scheduling
    auto& scheduler = world.getSystemScheduler();
    
    // Core ECS systems - organized by phases
    scheduler.addSystem<FlecsSystem<Lifetime>>(
        "LifetimeSystem", 
        lifetime_system
    ).inPhase("Logic");
    
    // Input systems - run in Input phase
    scheduler.addSystem<FlecsSystem<InputState, KeyboardInput, MouseInput, InputEvents>>(
        "InputSystem",
        input_processing_system
    ).inPhase("Input");
    
    // Camera systems - run in Logic phase with dependencies
    scheduler.addSystem<FlecsSystem<Camera>>(
        "CameraControlSystem",
        [](flecs::entity e, Camera& camera) {
            camera_control_system(e, camera, e.world().delta_time());
        }
    ).inPhase("Logic");
    
    scheduler.addSystem<FlecsSystem<Camera>>(
        "CameraMatrixSystem",
        camera_matrix_system
    ).inPhase("Logic");
    
    // Create input singleton entity and set window reference for accurate screen coordinates
    InputManager::createInputEntity(world.getFlecsWorld());
    InputManager::setWindow(window);
    
    // Create main camera entity
    CameraManager::createMainCamera(world.getFlecsWorld());
    
    // Set world reference in renderer for camera integration
    renderer.setWorld(&world.getFlecsWorld());
    
    // NOTE: Legacy CPU render system removed - entities now render directly via GPU compute
    
    // Configure profiler for 60 FPS
    Profiler::getInstance().setTargetFrameTime(TARGET_FRAME_TIME);

    // GPU stress test configuration - scale up to 10k for initial test
    constexpr size_t ENTITY_COUNT = 90000; // GPU can handle much more
    
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
    
    // Manual systems for game logic
    bool running = true; // Move declaration up here
    
    scheduler.addSystem<ManualSystem>(
        "ControlHandler",
        [&](flecs::world& flecsWorld, float deltaTime) {
            static bool initialized = false;
            if (!initialized) {
                ControlHandler::initialize(world);
                initialized = true;
            }
            ControlHandler::processControls(world, running, &renderer);
        }
    ).inPhase("Input");
    
    scheduler.addSystem<ManualSystem>(
        "GPUEntityUpload",
        [&](flecs::world& flecsWorld, float deltaTime) {
            if (renderer.getGPUEntityManager()) {
                renderer.uploadPendingGPUEntities();
                renderer.setDeltaTime(deltaTime);
            }
        }
    ).inPhase("Render");
    
    // Add system dependencies
    scheduler.addDependency("CameraMatrixSystem", "CameraControlSystem");
    scheduler.addDependency("GPUEntityUpload", "ControlHandler");
    
    // Initialize all systems through scheduler
    scheduler.initialize();
    
    std::cout << "\n🚀 Flecs System Scheduler initialized with " 
              << scheduler.getSystemCount() << " systems\n" << std::endl;
    int frameCount = 0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStartTime - lastFrameTime).count();
        lastFrameTime = frameStartTime;
        
        // Process SDL events through the input system
        InputManager::processSDLEvents(world.getFlecsWorld());
        
        // Systems now handle controls automatically through scheduler
        
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
        
        {
            PROFILE_SCOPE("ECS Update");
            // Systems handle GPU upload and deltaTime automatically
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

        // NOTE: Legacy CPU render system update removed - GPU entities render directly
        

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
        
        // Light frame pacing - let Vulkan present modes handle most of the timing
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        float frameTimeMs = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime).count();
        
        // Only cap if we're running way too fast (>90fps) to prevent GPU overheating
        constexpr float MIN_FRAME_TIME = 11.0f; // ~90fps cap
        if (frameTimeMs < MIN_FRAME_TIME) {
            float remainingMs = MIN_FRAME_TIME - frameTimeMs;
            if (remainingMs > 0.5f) {
                SDL_Delay(static_cast<int>(remainingMs));
            }
        }
    }


    renderer.cleanup();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}