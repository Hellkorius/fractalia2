#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <chrono>
#include "vulkan_renderer.h"
#include "PolygonFactory.h"
#include "ecs/world.hpp"
#include "ecs/systems/render_system.cpp"
#include "ecs/systems/physics_system.hpp"
#include "ecs/profiler.hpp"

int main(int argc, char* argv[]) {
    constexpr int TARGET_FPS = 60;
    constexpr float TARGET_FRAME_TIME = 1.0f / TARGET_FPS;
    
    
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

    // Use new Transform-based physics system
    world.getFlecsWorld().system<Transform, Velocity>()
        .each(movement_system);
        
    world.getFlecsWorld().system<Lifetime>()
        .each(lifetime_system);
    
    // Initialize render system
    RenderSystem renderSystem(&renderer, world.getFlecsWorld());
    
    // Configure profiler for 60 FPS
    Profiler::getInstance().setTargetFrameTime(TARGET_FRAME_TIME);

    // Create entities using new factory system
    auto triangleEntity = world.getEntityFactory().createTriangle(
        glm::vec3(-1.5f, 0.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
    );
    triangleEntity.set<Velocity>({{0.5f, 0.2f, 0.0f}, {0.0f, 0.0f, 1.0f}});

    auto squareEntity = world.getEntityFactory().createSquare(
        glm::vec3(1.5f, 0.0f, 0.0f),
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
    );
    squareEntity.set<Velocity>({{-0.3f, 0.4f, 0.0f}, {0.0f, 0.0f, -0.5f}});

    bool running = true;
    SDL_Event event;


    int frameCount = 0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStartTime - lastFrameTime).count();
        lastFrameTime = frameStartTime;
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_P) {
                    // Print performance report
                    Profiler::getInstance().printReport();
                }
            }
        }

        // Profile the main update loop
        PROFILE_BEGIN_FRAME();
        {
            PROFILE_SCOPE("ECS Update");
            world.progress(deltaTime);
        }

        {
            PROFILE_SCOPE("Render System");
            renderSystem.update();
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
            auto memStats = world.getMemoryManager().getStats();
            std::cout << "Frame " << frameCount 
                      << ": Avg " << avgFrameTime << "ms"
                      << " (" << (1000.0f / avgFrameTime) << " FPS)"
                      << " | Memory: " << (memStats.totalAllocated / 1024) << "KB"
                      << std::endl;
        }
        
        // Cap framerate at TARGET_FPS
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float>(frameEndTime - frameStartTime).count();
        if (frameTime < TARGET_FRAME_TIME) {
            int delayMs = static_cast<int>((TARGET_FRAME_TIME - frameTime) * 1000.0f);
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