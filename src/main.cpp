#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
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
    pos.x += vel.x * 0.016f; // Approximate 60 FPS delta time
    pos.y += vel.y * 0.016f;
    pos.z += vel.z * 0.016f;
    
    // Bounce off screen edges
    if (pos.x > 2.0f || pos.x < -2.0f) vel.x = -vel.x;
    if (pos.y > 1.5f || pos.y < -1.5f) vel.y = -vel.y;
}

void rotation_system(flecs::entity e, Position& pos, Rotation& rot) {
    const float deltaTime = 0.016f; // Approximate 60 FPS delta time
    
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
    constexpr int FRAME_DELAY_MS = 1000 / TARGET_FPS;
    
    std::cout << "Starting Fractalia2..." << std::endl;
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    std::cout << "SDL initialized successfully" << std::endl;

    // Check Vulkan support
    uint32_t extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensions || extensionCount == 0) {
        std::cerr << "Vulkan is not supported or no Vulkan extensions available" << std::endl;
        std::cerr << "Make sure Vulkan drivers are installed" << std::endl;
        SDL_Quit();
        return -1;
    }
    
    std::cout << "Vulkan support detected. Available extensions (" << extensionCount << "):" << std::endl;
    for (uint32_t i = 0; i < extensionCount; i++) {
        std::cout << "  " << extensions[i] << std::endl;
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
    
    std::cout << "Window created successfully" << std::endl;

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

    std::cout << "Fractalia2 initialized successfully!" << std::endl;
    std::cout << "SDL3 Version: " << SDL_GetVersion() << std::endl;
    std::cout << "Vulkan renderer initialized" << std::endl;
    std::cout << "Starting main loop..." << std::endl;
    std::cout.flush();

    int frameCount = 0;
    while (running) {
        Uint64 frameStartTime = SDL_GetTicks();
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                std::cout << "Quit event received" << std::endl;
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                std::cout << "Escape key pressed" << std::endl;
                running = false;
            }
        }

        world.progress();

        // Collect all entities with position and shape components for rendering
        std::vector<std::tuple<glm::vec3, VulkanRenderer::ShapeType, glm::vec4>> renderEntities;
        
        world.query<Position, Shape, Color>().each([&](flecs::entity e, Position& pos, Shape& shape, Color& color) {
            VulkanRenderer::ShapeType vkShapeType = (shape.type == ShapeType::Triangle) ? 
                VulkanRenderer::ShapeType::Triangle : VulkanRenderer::ShapeType::Square;
            renderEntities.emplace_back(
                glm::vec3(pos.x, pos.y, pos.z),
                vkShapeType,
                glm::vec4(color.r, color.g, color.b, color.a)
            );
            if (frameCount % 60 == 0) {
                std::cout << "Found entity: pos(" << pos.x << ", " << pos.y << ", " << pos.z << ") ";
                std::cout << "shape: " << (shape.type == ShapeType::Triangle ? "Triangle" : "Square") << std::endl;
            }
        });
        
        if (frameCount % 60 == 0) {
            std::cout << "Collected " << renderEntities.size() << " entities for rendering" << std::endl;
        }
        
        renderer.updateEntities(renderEntities);
        
        if (frameCount % 60 == 0) {
            std::cout << "Rendering " << renderEntities.size() << " entities" << std::endl;
            for (size_t i = 0; i < renderEntities.size(); i++) {
                auto& [pos, shapeType, color] = renderEntities[i];
                std::cout << "Entity " << i << ": pos(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
                std::cout << " shape: " << (shapeType == VulkanRenderer::ShapeType::Triangle ? "Triangle" : "Square") << std::endl;
            }
        }

        if (frameCount == 0) {
            std::cout << "Drawing first frame..." << std::endl;
            std::cout.flush();
        }

        renderer.drawFrame();

        frameCount++;
        if (frameCount == 1) {
            std::cout << "First frame drawn successfully" << std::endl;
        } else if (frameCount % 60 == 0) {
            std::cout << "Frame " << frameCount << " drawn" << std::endl;
        }
        
        // Cap framerate at TARGET_FPS
        Uint64 frameTime = SDL_GetTicks() - frameStartTime;
        if (frameTime < FRAME_DELAY_MS) {
            SDL_Delay(FRAME_DELAY_MS - frameTime);
        }
    }

    std::cout << "Exiting main loop..." << std::endl;

    renderer.cleanup();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}