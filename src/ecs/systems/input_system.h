#pragma once

#include "../component.h"
#include <SDL3/SDL.h>
#include <flecs.h>
#include <glm/glm.hpp>

// Input context to hold persistent state (replaces global variables)
struct InputContext {
    glm::vec2 previousMousePos{0.0f, 0.0f};
    bool mouseInitialized = false;
    SDL_Window* window = nullptr;
};

// Input processing system - handles SDL events and updates input components
void input_processing_system(flecs::entity e, InputState& state, KeyboardInput& keyboard, 
                             MouseInput& mouse, InputEvents& events);

// Input manager functions for manual control
namespace InputManager {
    // Initialize input singleton entity and context
    flecs::entity createInputEntity(flecs::world& world);
    
    // Get the input context instance
    InputContext& getContext();
    
    // Set the window reference for accurate screen size calculations
    void setWindow(SDL_Window* window);
    
    // Process SDL events manually (call this once per frame before ECS update)
    void processSDLEvents(flecs::world& world);
    
    // Helper functions for coordinate transformation
    glm::vec2 screenToWorld(const glm::vec2& screenPos, flecs::world& world);
}

// Helper functions for input queries
namespace InputQuery {
    bool isKeyDown(flecs::world& world, int scancode);
    bool isKeyPressed(flecs::world& world, int scancode);
    bool isKeyReleased(flecs::world& world, int scancode);
    
    bool isMouseButtonDown(flecs::world& world, int button);
    bool isMouseButtonPressed(flecs::world& world, int button);
    bool isMouseButtonReleased(flecs::world& world, int button);
    
    glm::vec2 getMousePosition(flecs::world& world);
    glm::vec2 getMouseWorldPosition(flecs::world& world);
    glm::vec2 getMouseDelta(flecs::world& world);
    glm::vec2 getMouseWheelDelta(flecs::world& world);
    
    bool shouldQuit(flecs::world& world);
}