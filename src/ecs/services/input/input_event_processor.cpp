#include "input_event_processor.h"
#include <iostream>
#include <algorithm>

InputEventProcessor::InputEventProcessor() {
}

InputEventProcessor::~InputEventProcessor() {
    cleanup();
}

bool InputEventProcessor::initialize(SDL_Window* window) {
    if (initialized) {
        return true;
    }
    
    this->window = window;
    
    // Clear all state
    std::fill(keyboardState.keys, keyboardState.keys + KeyboardState::MAX_KEYS, false);
    std::fill(keyboardState.keysPressed, keyboardState.keysPressed + KeyboardState::MAX_KEYS, false);
    std::fill(keyboardState.keysReleased, keyboardState.keysReleased + KeyboardState::MAX_KEYS, false);
    keyboardState.shift = false;
    keyboardState.ctrl = false;
    keyboardState.alt = false;
    
    std::fill(mouseState.buttons, mouseState.buttons + MouseState::MAX_BUTTONS, false);
    std::fill(mouseState.buttonsPressed, mouseState.buttonsPressed + MouseState::MAX_BUTTONS, false);
    std::fill(mouseState.buttonsReleased, mouseState.buttonsReleased + MouseState::MAX_BUTTONS, false);
    mouseState.position = glm::vec2(0.0f);
    mouseState.delta = glm::vec2(0.0f);
    mouseState.wheelDelta = glm::vec2(0.0f);
    
    hasWindowResize = false;
    windowResizeWidth = 0;
    windowResizeHeight = 0;
    quitRequested = false;
    inputConsumed = false;
    
    initialized = true;
    return true;
}

void InputEventProcessor::cleanup() {
    if (!initialized) {
        return;
    }
    
    window = nullptr;
    initialized = false;
}

void InputEventProcessor::processSDLEvents() {
    if (!initialized) {
        return;
    }
    
    // Clear frame-based input states from previous frame
    std::fill(keyboardState.keysPressed, keyboardState.keysPressed + KeyboardState::MAX_KEYS, false);
    std::fill(keyboardState.keysReleased, keyboardState.keysReleased + KeyboardState::MAX_KEYS, false);
    std::fill(mouseState.buttonsPressed, mouseState.buttonsPressed + MouseState::MAX_BUTTONS, false);
    std::fill(mouseState.buttonsReleased, mouseState.buttonsReleased + MouseState::MAX_BUTTONS, false);
    
    // Clear frame-based deltas
    mouseState.delta = glm::vec2(0.0f);
    mouseState.wheelDelta = glm::vec2(0.0f);
    
    // Clear window events
    hasWindowResize = false;
    
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                quitRequested = true;
                break;
                
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                handleKeyboardEvent(event);
                break;
                
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButtonEvent(event);
                break;
                
            case SDL_EVENT_MOUSE_MOTION:
                handleMouseMotionEvent(event);
                break;
                
            case SDL_EVENT_MOUSE_WHEEL:
                handleMouseWheelEvent(event);
                break;
                
            case SDL_EVENT_WINDOW_RESIZED:
                handleWindowEvent(event);
                break;
        }
    }
    
}

bool InputEventProcessor::isKeyDown(int scancode) const {
    if (!initialized || scancode < 0 || scancode >= static_cast<int>(KeyboardState::MAX_KEYS)) {
        return false;
    }
    return keyboardState.keys[scancode];
}

bool InputEventProcessor::isKeyPressed(int scancode) const {
    if (!initialized || scancode < 0 || scancode >= static_cast<int>(KeyboardState::MAX_KEYS)) {
        return false;
    }
    return keyboardState.keysPressed[scancode];
}

bool InputEventProcessor::isKeyReleased(int scancode) const {
    if (!initialized || scancode < 0 || scancode >= static_cast<int>(KeyboardState::MAX_KEYS)) {
        return false;
    }
    return keyboardState.keysReleased[scancode];
}

bool InputEventProcessor::isMouseButtonDown(int button) const {
    if (!initialized || button <= 0 || (button - 1) >= static_cast<int>(MouseState::MAX_BUTTONS)) {
        return false;
    }
    return mouseState.buttons[button - 1]; // Convert SDL 1-based to 0-based array index
}

bool InputEventProcessor::isMouseButtonPressed(int button) const {
    if (!initialized || button <= 0 || (button - 1) >= static_cast<int>(MouseState::MAX_BUTTONS)) {
        return false;
    }
    return mouseState.buttonsPressed[button - 1]; // Convert SDL 1-based to 0-based array index
}

bool InputEventProcessor::isMouseButtonReleased(int button) const {
    if (!initialized || button <= 0 || (button - 1) >= static_cast<int>(MouseState::MAX_BUTTONS)) {
        return false;
    }
    return mouseState.buttonsReleased[button - 1]; // Convert SDL 1-based to 0-based array index
}

glm::vec2 InputEventProcessor::getMousePosition() const {
    return initialized ? mouseState.position : glm::vec2(0.0f);
}

glm::vec2 InputEventProcessor::getMouseDelta() const {
    return initialized ? mouseState.delta : glm::vec2(0.0f);
}

glm::vec2 InputEventProcessor::getMouseWheelDelta() const {
    return initialized ? mouseState.wheelDelta : glm::vec2(0.0f);
}

bool InputEventProcessor::hasWindowResizeEvent(int& width, int& height) const {
    if (hasWindowResize) {
        width = windowResizeWidth;
        height = windowResizeHeight;
        return true;
    }
    return false;
}

bool InputEventProcessor::shouldQuit() const {
    return quitRequested;
}

void InputEventProcessor::handleKeyboardEvent(const SDL_Event& event) {
    int scancode = event.key.scancode;
    bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
    
    if (scancode >= 0 && scancode < static_cast<int>(KeyboardState::MAX_KEYS)) {
        if (pressed && !keyboardState.keys[scancode]) {
            keyboardState.keysPressed[scancode] = true;
            std::cout << "Key scancode " << scancode << " pressed! (SDL_SCANCODE_EQUALS=" << SDL_SCANCODE_EQUALS << ")" << std::endl;
        } else if (!pressed && keyboardState.keys[scancode]) {
            keyboardState.keysReleased[scancode] = true;
        }
        keyboardState.keys[scancode] = pressed;
    }
    
    // Update modifier states
    keyboardState.shift = (SDL_GetModState() & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)) != 0;
    keyboardState.ctrl = (SDL_GetModState() & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)) != 0;
    keyboardState.alt = (SDL_GetModState() & (SDL_KMOD_LALT | SDL_KMOD_RALT)) != 0;
}

void InputEventProcessor::handleMouseButtonEvent(const SDL_Event& event) {
    int button = event.button.button - 1; // SDL uses 1-based indexing, convert to 0-based for array
    bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    
    if (button >= 0 && button < static_cast<int>(MouseState::MAX_BUTTONS)) {
        if (pressed && !mouseState.buttons[button]) {
            mouseState.buttonsPressed[button] = true;
            std::cout << "Mouse button " << (button + 1) << " (SDL_BUTTON_" << (button + 1) << ") pressed!" << std::endl;
        } else if (!pressed && mouseState.buttons[button]) {
            mouseState.buttonsReleased[button] = true;
        }
        mouseState.buttons[button] = pressed;
    }
}

void InputEventProcessor::handleMouseMotionEvent(const SDL_Event& event) {
    mouseState.position.x = static_cast<float>(event.motion.x);
    mouseState.position.y = static_cast<float>(event.motion.y);
    mouseState.delta.x = static_cast<float>(event.motion.xrel);
    mouseState.delta.y = static_cast<float>(event.motion.yrel);
}

void InputEventProcessor::handleMouseWheelEvent(const SDL_Event& event) {
    mouseState.wheelDelta.x = static_cast<float>(event.wheel.x);
    mouseState.wheelDelta.y = static_cast<float>(event.wheel.y);
}

void InputEventProcessor::handleWindowEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        hasWindowResize = true;
        windowResizeWidth = event.window.data1;
        windowResizeHeight = event.window.data2;
    }
}