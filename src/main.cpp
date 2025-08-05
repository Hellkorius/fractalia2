#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <chrono>
#include "vulkan_renderer.h"
#include "PolygonFactory.h"

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Color {
    float r, g, b, a;
};

enum class ShapeType {
    Triangle,
    Square
};

struct Shape {
    ShapeType type;
};

struct Rotation {
    glm::vec3 axis;
    float angle;
};

void movement_system(flecs::entity e, Position& pos, Velocity& vel) {
    // Get delta time from the world (Flecs provides this)
    const float deltaTime = e.world().delta_time();
    
    pos.x += vel.x * deltaTime;
    pos.y += vel.y * deltaTime;
    pos.z += vel.z * deltaTime;
    
    // Bounce off screen edges
    if (pos.x > 2.0f || pos.x < -2.0f) vel.x = -vel.x;
    if (pos.y > 1.5f || pos.y < -1.5f) vel.y = -vel.y;
}

void rotation_system(flecs::entity e, Position& pos, Rotation& rot) {
    // Get delta time from the world
    const float deltaTime = e.world().delta_time();
    
    // Update rotation angle
    rot.angle += deltaTime * 2.0f; // Rotate at 2 radians per second
    
    // Keep angle in [0, 2π] range
    if (rot.angle > 2.0f * M_PI) {
        rot.angle -= 2.0f * M_PI;
    }
    
    // Create rotation matrix and apply to position for orbital movement
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), rot.angle * 0.5f, rot.axis);
    glm::vec4 rotatedPos = rotationMatrix * glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);
    
    // Add orbital motion to base position
    pos.x = rotatedPos.x;
    pos.y = rotatedPos.y;
}

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

    flecs::world world;

    world.system<Position, Velocity>()
        .each(movement_system);

    world.system<Position, Rotation>()
        .each(rotation_system);
    
    // Cache the query for better performance
    auto renderQuery = world.query<Position, Shape, Color>();

    // Create a triangle entity
    auto triangleEntity = world.entity()
        .set<Position>({-1.5f, 0.0f, 0.0f})
        .set<Color>({1.0f, 1.0f, 1.0f, 1.0f})
        .set<Shape>({ShapeType::Triangle});

    // Create a square entity (using triangle geometry for testing)
    auto squareEntity = world.entity()
        .set<Position>({1.5f, 0.0f, 0.0f})
        .set<Color>({1.0f, 0.0f, 0.0f, 1.0f})
        .set<Shape>({ShapeType::Square});

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
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        world.progress(deltaTime);

        // Collect all entities with position and shape components for rendering
        std::vector<std::tuple<glm::vec3, VulkanRenderer::ShapeType, glm::vec4>> renderEntities;
        renderEntities.reserve(32); // Pre-allocate for better performance
        
        renderQuery.each([&](flecs::entity e, Position& pos, Shape& shape, Color& color) {
            VulkanRenderer::ShapeType vkShapeType = (shape.type == ShapeType::Triangle) ? 
                VulkanRenderer::ShapeType::Triangle : VulkanRenderer::ShapeType::Square;
            renderEntities.emplace_back(
                glm::vec3(pos.x, pos.y, pos.z),
                vkShapeType,
                glm::vec4(color.r, color.g, color.b, color.a)
            );
        });
        
        
        renderer.updateEntities(renderEntities);
        


        renderer.drawFrame();

        frameCount++;
        
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