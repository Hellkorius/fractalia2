#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <chrono>
#include <thread>

#include "vulkan_renderer.h"
#include "ecs/debug.h"
#include "PolygonFactory.h"
#include <flecs.h>
#include "ecs/entity_factory.h"
#include "ecs/system_scheduler.h"
#include "ecs/systems/lifetime_system.h"
#include "ecs/systems/input_system.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/simple_control_system.h"
#include "ecs/movement_command_system.h"
#include "ecs/camera_component.h"
#include "ecs/component.h"
#include "ecs/profiler.h"
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

    flecs::world world;
    
    // Create entity factory and system scheduler directly
    EntityFactory entityFactory(world);
    SystemScheduler scheduler(world);
    
    // Initialize scheduler first (creates phases and globals)
    scheduler.initialize();
    
    // Direct Flecs system registration with phases
    
    // Note: Input processing is handled by InputManager::processSDLEvents() in main loop
    // No input system registration needed since it's done manually
    
    // Camera systems - registered with input phase for proper ordering
    world.system<Camera>("CameraControlSystem")
        .each([](flecs::entity e, Camera& camera) {
            camera_control_system(e, camera, e.world().delta_time());
        })
        .child_of(scheduler.getInputPhase());
    
    world.system<Camera>("CameraMatrixSystem")
        .each(camera_matrix_system)
        .child_of(scheduler.getLogicPhase());
        
    DEBUG_LOG("Camera systems registered");
    
    // Physics/lifetime - third phase
    world.system<Lifetime>("LifetimeSystem")
        .each(lifetime_system)
        .child_of(scheduler.getPhysicsPhase());
    
    // Create input singleton entity and set window reference for accurate screen coordinates
    InputManager::createInputEntity(world);
    InputManager::setWindow(window);
    
    // Create simple camera entity
    world.entity("MainCamera").set<Camera>({});
    DEBUG_LOG("Camera entities: " << world.count<Camera>());
    
    // Set world reference in renderer for camera integration
    renderer.setWorld(&world);
    
    // NOTE: Legacy CPU render system removed - entities now render directly via GPU compute
    
    // Configure profiler for 60 FPS
    Profiler::getInstance().setTargetFrameTime(TARGET_FRAME_TIME);

    // GPU stress test configuration - scale up to 10k for initial test
    constexpr size_t ENTITY_COUNT = 80000; // GPU can handle much more
    
    DEBUG_LOG("Creating " << ENTITY_COUNT << " GPU entities for stress testing...");
    
    // Create a swarm of entities for stress testing - spawn away from center
    auto swarmEntities = entityFactory.createSwarm(
        ENTITY_COUNT,
        glm::vec3(10.0f, 10.0f, 0.0f), // spawn far from center
        8.0f  // much larger radius for wide spread
    );
    
    // Upload all entities to GPU immediately (before Flecs systems are initialized)
    renderer.getGPUEntityManager()->addEntitiesFromECS(swarmEntities);
    renderer.uploadPendingGPUEntities(); // Ensure they're actually uploaded to buffers
    
    DEBUG_LOG("Created " << swarmEntities.size() << " GPU entities!");
    
    // Initialize simple Flecs control system (just handles input) with Input phase
    bool running = true;
    SimpleControlSystem::initialize(world);
    
    DEBUG_LOG("\n🚀 Simple Flecs systems ready\n");
    int frameCount = 0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStartTime - lastFrameTime).count();
        lastFrameTime = frameStartTime;
        
        // Hard-cap deltaTime to prevent frame-rate sensitive collision math explosions
        deltaTime = std::min(deltaTime, 1.0f / 30.0f);  // never exceed 33 ms
        
        // Process SDL events through the input system
        InputManager::processSDLEvents(world);
        
        // Check application state from Flecs systems
        auto* appState = world.get<ApplicationState>();
        if (appState && (appState->requestQuit || !appState->running)) {
            running = false;
        }
        
        // Update renderer delta time for GPU compute
        renderer.setDeltaTime(deltaTime);
        
        // Handle GPU operations based on control state (after Flecs systems)
        SimpleControlSystem::processControlActions(world, renderer, entityFactory);
        
        // Systems now handle controls automatically through scheduler
        
        // Handle window resize for camera aspect ratio
        auto inputEntity = world.lookup("InputManager");
        if (inputEntity.is_valid()) {
            auto* events = inputEntity.get<InputEvents>();
            if (events) {
                for (size_t i = 0; i < events->eventCount; i++) {
                    const auto& event = events->events[i];
                    if (event.type == InputEvents::Event::WINDOW_RESIZE) {
                        renderer.updateAspectRatio(event.windowResizeEvent.width, event.windowResizeEvent.height);
                        DEBUG_LOG("Window resized to " << event.windowResizeEvent.width << "x" << event.windowResizeEvent.height);
                    }
                }
            }
        }

        // Profile the main update loop
        PROFILE_BEGIN_FRAME();
        
        {
            PROFILE_SCOPE("ECS Update");
            // Simple Flecs execution
            world.progress(deltaTime);
        }
        
        // Clear input frame states AFTER all systems have processed input
        {
            PROFILE_SCOPE("Input Cleanup");
            auto inputEntity = world.lookup("InputManager");
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
            size_t activeEntities = static_cast<size_t>(world.count<Transform>());
            size_t estimatedMemory = activeEntities * (sizeof(Transform) + sizeof(Renderable) + sizeof(MovementPattern));
            
            // Update profiler with estimated memory usage
            Profiler::getInstance().updateMemoryUsage(estimatedMemory);
            
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            std::cout << "Frame " << frameCount 
                      << ": Avg " << avgFrameTime << "ms"
                      << " (" << fps << " FPS)"
                      << " | Entities: " << activeEntities
                      << " | Est Memory: " << (estimatedMemory / 1024) << "KB"
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