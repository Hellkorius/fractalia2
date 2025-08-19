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
    
    EntityFactory entityFactory(world);
    SystemScheduler scheduler(world);
    
    scheduler.initialize();
    
    // Direct Flecs system registration with phases
    
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
    
    world.system<Lifetime>("LifetimeSystem")
        .each(lifetime_system)
        .child_of(scheduler.getPhysicsPhase());
    
    InputManager::createInputEntity(world);
    InputManager::setWindow(window);
    
    CameraManager::createMainCamera(world);
    DEBUG_LOG("Camera entities: " << world.count<Camera>());
    
    renderer.setWorld(&world);
    
    Profiler::getInstance().setTargetFrameTime(TARGET_FRAME_TIME);

    constexpr size_t ENTITY_COUNT = 1000;
    
    DEBUG_LOG("Creating " << ENTITY_COUNT << " GPU entities for stress testing...");
    
    auto swarmEntities = entityFactory.createSwarm(
        ENTITY_COUNT,
        glm::vec3(10.0f, 10.0f, 0.0f),
        8.0f
    );
    
    auto* gpuEntityManager = renderer.getGPUEntityManager();
    gpuEntityManager->addEntitiesFromECS(swarmEntities);
    gpuEntityManager->uploadPendingEntities();
    
    DEBUG_LOG("Created " << swarmEntities.size() << " GPU entities!");
    
    bool running = true;
    SimpleControlSystem::initialize(world);
    
    DEBUG_LOG("\n🚀 Simple Flecs systems ready\n");
    int frameCount = 0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStartTime - lastFrameTime).count();
        lastFrameTime = frameStartTime;
        
        deltaTime = std::min(deltaTime, 1.0f / 30.0f);
        
        InputManager::processSDLEvents(world);
        
        auto* appState = world.get<ApplicationState>();
        if (appState && (appState->requestQuit || !appState->running)) {
            running = false;
        }
        
        renderer.setDeltaTime(deltaTime);
        
        SimpleControlSystem::processControlActions(world, renderer, entityFactory);
        
        // Handle window resize for camera aspect ratio
        auto inputEntity = world.lookup("InputManager");
        if (inputEntity.is_valid()) {
            auto* events = inputEntity.get<InputEvents>();
            if (events) {
                for (size_t i = 0; i < events->eventCount; i++) {
                    const auto& event = events->events[i];
                    if (event.type == InputEvents::Event::WINDOW_RESIZE) {
                        renderer.updateAspectRatio(event.windowResizeEvent.width, event.windowResizeEvent.height);
                        renderer.setFramebufferResized(true);
                        DEBUG_LOG("Window resized to " << event.windowResizeEvent.width << "x" << event.windowResizeEvent.height);
                    }
                }
            }
        }

        PROFILE_BEGIN_FRAME();
        
        {
            PROFILE_SCOPE("ECS Update");
            world.progress(deltaTime);
        }
        
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

        {
            PROFILE_SCOPE("Vulkan Rendering");
            renderer.drawFrame();
        }

        frameCount++;
        PROFILE_END_FRAME();
        
        if (frameCount % 300 == 0) {
            float avgFrameTime = Profiler::getInstance().getFrameTime();
            size_t activeEntities = static_cast<size_t>(world.count<Transform>());
            size_t estimatedMemory = activeEntities * (sizeof(Transform) + sizeof(Renderable) + sizeof(MovementPattern));
            
            Profiler::getInstance().updateMemoryUsage(estimatedMemory);
            
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            std::cout << "Frame " << frameCount 
                      << ": Avg " << avgFrameTime << "ms"
                      << " (" << fps << " FPS)"
                      << " | Entities: " << activeEntities
                      << " | Est Memory: " << (estimatedMemory / 1024) << "KB"
                      << std::endl;
        }
        
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        float frameTimeMs = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime).count();
        
        constexpr float MIN_FRAME_TIME = 11.0f;
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