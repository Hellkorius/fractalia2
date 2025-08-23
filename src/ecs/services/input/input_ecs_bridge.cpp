#include "input_ecs_bridge.h"
#include "../camera_service.h"
#include "../../components/component.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <algorithm>

InputECSBridge::InputECSBridge() {
}

InputECSBridge::~InputECSBridge() {
    cleanup();
}

bool InputECSBridge::initialize(flecs::world& world) {
    if (initialized) {
        return true;
    }
    
    this->world = &world;
    
    // Create input entity directly
    inputEntity = world.entity("InputEntity")
        .add<InputState>()
        .add<KeyboardInput>()
        .add<MouseInput>()
        .add<InputEvents>();
    
    if (!inputEntity.is_valid()) {
        std::cerr << "InputECSBridge: Failed to create input entity" << std::endl;
        return false;
    }
    
    frameNumber = 0;
    initialized = true;
    return true;
}

void InputECSBridge::cleanup() {
    if (!initialized) {
        return;
    }
    
    world = nullptr;
    inputEntity = flecs::entity::null();
    initialized = false;
}

void InputECSBridge::synchronizeToECSComponents(const KeyboardState& keyboardState, 
                                               const MouseState& mouseState,
                                               float deltaTime) {
    if (!initialized || !inputEntity.is_valid()) {
        return;
    }
    
    // Update InputState component
    auto* inputState = inputEntity.get_mut<InputState>();
    if (inputState) {
        inputState->deltaTime = deltaTime;
        inputState->frameNumber = ++frameNumber;
        // quit flag is set by InputEventProcessor
    }
    
    // Update KeyboardInput component
    auto* keyboardInput = inputEntity.get_mut<KeyboardInput>();
    if (keyboardInput) {
        // Copy internal keyboard state to ECS component
        std::copy(keyboardState.keys, keyboardState.keys + KeyboardState::MAX_KEYS, 
                 keyboardInput->keys);
        std::copy(keyboardState.keysPressed, keyboardState.keysPressed + KeyboardState::MAX_KEYS,
                 keyboardInput->keysPressed);
        std::copy(keyboardState.keysReleased, keyboardState.keysReleased + KeyboardState::MAX_KEYS,
                 keyboardInput->keysReleased);
        keyboardInput->shift = keyboardState.shift;
        keyboardInput->ctrl = keyboardState.ctrl;
        keyboardInput->alt = keyboardState.alt;
    }
    
    // Update MouseInput component  
    auto* mouseInput = inputEntity.get_mut<MouseInput>();
    if (mouseInput) {
        // Copy internal mouse state to ECS component
        std::copy(mouseState.buttons, mouseState.buttons + MouseState::MAX_BUTTONS,
                 mouseInput->buttons);
        std::copy(mouseState.buttonsPressed, mouseState.buttonsPressed + MouseState::MAX_BUTTONS,
                 mouseInput->buttonsPressed);
        std::copy(mouseState.buttonsReleased, mouseState.buttonsReleased + MouseState::MAX_BUTTONS,
                 mouseInput->buttonsReleased);
        mouseInput->position = mouseState.position;
        mouseInput->deltaPosition = mouseState.delta;
        mouseInput->wheelDelta = mouseState.wheelDelta;
    }
}

glm::vec2 InputECSBridge::getMouseWorldPosition(const MouseState& mouseState, CameraService* cameraService, SDL_Window* window) const {
    if (!initialized || !cameraService || !window) {
        return glm::vec2(0.0f);
    }
    
    // Get actual window size from SDL
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    glm::vec2 screenSize(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    
    // Use CameraService to convert screen coordinates to world coordinates
    return cameraService->screenToWorld(mouseState.position, screenSize);
}

KeyboardInput* InputECSBridge::getKeyboardInput() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<KeyboardInput>();
}

MouseInput* InputECSBridge::getMouseInput() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<MouseInput>();
}

InputState* InputECSBridge::getInputState() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<InputState>();
}

InputEvents* InputECSBridge::getInputEvents() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<InputEvents>();
}