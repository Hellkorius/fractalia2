#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

// Input state structures
struct KeyboardState {
    static constexpr size_t MAX_KEYS = 512;
    bool keys[MAX_KEYS] = {false};
    bool keysPressed[MAX_KEYS] = {false};
    bool keysReleased[MAX_KEYS] = {false};
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

struct MouseState {
    static constexpr size_t MAX_BUTTONS = 8;
    bool buttons[MAX_BUTTONS] = {false};
    bool buttonsPressed[MAX_BUTTONS] = {false};
    bool buttonsReleased[MAX_BUTTONS] = {false};
    glm::vec2 position{0.0f};
    glm::vec2 delta{0.0f};
    glm::vec2 wheelDelta{0.0f};
};

// Input event processor - handles SDL events and maintains raw input state
class InputEventProcessor {
public:
    InputEventProcessor();
    ~InputEventProcessor();
    
    // Initialization
    bool initialize(SDL_Window* window);
    void cleanup();
    
    // Event processing
    void processSDLEvents();
    
    // Raw input queries
    bool isKeyDown(int scancode) const;
    bool isKeyPressed(int scancode) const;
    bool isKeyReleased(int scancode) const;
    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonReleased(int button) const;
    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseDelta() const;
    glm::vec2 getMouseWheelDelta() const;
    
    // Window event handling
    bool hasWindowResizeEvent(int& width, int& height) const;
    bool shouldQuit() const;
    
    // State access
    const KeyboardState& getKeyboardState() const { return keyboardState; }
    const MouseState& getMouseState() const { return mouseState; }
    
    // Input consumption (for UI systems)
    bool isInputConsumed() const { return inputConsumed; }
    void setInputConsumed(bool consumed) { inputConsumed = consumed; }

private:
    SDL_Window* window = nullptr;
    bool initialized = false;
    bool inputConsumed = false;
    
    // Window event state
    bool hasWindowResize = false;
    int windowResizeWidth = 0;
    int windowResizeHeight = 0;
    bool quitRequested = false;
    
    // Input state
    KeyboardState keyboardState;
    MouseState mouseState;
    
    // SDL event handling
    void handleKeyboardEvent(const SDL_Event& event);
    void handleMouseButtonEvent(const SDL_Event& event);
    void handleMouseMotionEvent(const SDL_Event& event);
    void handleMouseWheelEvent(const SDL_Event& event);
    void handleWindowEvent(const SDL_Event& event);
};