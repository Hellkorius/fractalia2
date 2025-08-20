#pragma once

#include <flecs.h>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include "input_event_processor.h" // For KeyboardState, MouseState definitions

// Forward declarations
class InputActionSystem;
class CameraService;
struct KeyboardInput;
struct MouseInput;
struct InputState;
struct InputEvents;

// Input ECS bridge - handles ECS component synchronization
class InputECSBridge {
public:
    InputECSBridge();
    ~InputECSBridge();
    
    // Initialization
    bool initialize(flecs::world& world);
    void cleanup();
    
    // ECS synchronization
    void synchronizeToECSComponents(const KeyboardState& keyboardState, 
                                   const MouseState& mouseState,
                                   float deltaTime);
    
    // World coordinate conversion
    glm::vec2 getMouseWorldPosition(const MouseState& mouseState, CameraService* cameraService, SDL_Window* window) const;
    
    // ECS component access
    KeyboardInput* getKeyboardInput() const;
    MouseInput* getMouseInput() const;
    InputState* getInputState() const;
    InputEvents* getInputEvents() const;

private:
    flecs::world* world = nullptr;
    flecs::entity inputEntity;
    bool initialized = false;
    
    // Frame tracking
    uint64_t frameNumber = 0;
};