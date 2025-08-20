#pragma once

#include "../core/service_locator.h"
#include "../components/component.h"
#include <SDL3/SDL.h>
#include <flecs.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

// Forward declarations
struct InputBinding;
struct InputAction;
struct InputContext;
class CameraService;

// Input action types
enum class InputActionType {
    DIGITAL,    // On/Off (keyboard keys, mouse buttons)
    ANALOG_1D,  // Single axis (mouse wheel, trigger)
    ANALOG_2D   // Two axes (mouse position, stick)
};

// Input action state
struct InputActionState {
    InputActionType type;
    bool digitalValue = false;
    float analogValue1D = 0.0f;
    glm::vec2 analogValue2D{0.0f, 0.0f};
    bool justPressed = false;
    bool justReleased = false;
    float duration = 0.0f;  // How long the action has been active
};

// Input binding - maps physical input to logical action
struct InputBinding {
    enum class InputType {
        KEYBOARD_KEY,
        MOUSE_BUTTON,
        MOUSE_AXIS_X,
        MOUSE_AXIS_Y,
        MOUSE_WHEEL_X,
        MOUSE_WHEEL_Y
    };
    
    InputType inputType;
    union {
        int keycode;        // For keyboard
        int mouseButton;    // For mouse buttons
        int axisIndex;      // For analog inputs
    };
    
    // Modifiers and settings
    bool requiresShift = false;
    bool requiresCtrl = false;
    bool requiresAlt = false;
    float deadzone = 0.1f;      // For analog inputs
    float sensitivity = 1.0f;   // Multiplier for analog inputs
    bool invertAxis = false;    // Invert analog input
    
    InputBinding(InputType type, int value) : inputType(type) {
        switch (type) {
            case InputType::KEYBOARD_KEY:
                keycode = value;
                break;
            case InputType::MOUSE_BUTTON:
                mouseButton = value;
                break;
            default:
                axisIndex = value;
                break;
        }
    }
};

// Input context - groups of bindings that can be switched
struct InputContextDefinition {
    std::string name;
    std::unordered_map<std::string, std::vector<InputBinding>> actionBindings;
    int priority = 0;  // Higher priority contexts override lower ones
    bool active = true;
};

// Input action definition
struct InputActionDefinition {
    std::string name;
    InputActionType type;
    std::string description;
    std::vector<InputBinding> defaultBindings;
};

// Input service - centralized input management
class InputService {
public:
    DECLARE_SERVICE(InputService);
    
    InputService();
    ~InputService();
    
    // Initialization and cleanup
    bool initialize(flecs::world& world, SDL_Window* window);
    void cleanup();
    
    // Frame processing
    void processFrame(float deltaTime);
    void processSDLEvents();
    
    // Context management
    void registerContext(const std::string& name, int priority = 0);
    void setContextActive(const std::string& contextName, bool active);
    void pushContext(const std::string& contextName);  // Temporarily activate
    void popContext();  // Return to previous context stack
    std::string getCurrentContext() const;
    
    // Action system
    void registerAction(const InputActionDefinition& actionDef);
    void bindAction(const std::string& contextName, const std::string& actionName, const InputBinding& binding);
    void unbindAction(const std::string& contextName, const std::string& actionName);
    void clearActionBindings(const std::string& actionName);
    
    // Action queries
    bool isActionActive(const std::string& actionName) const;
    bool isActionJustPressed(const std::string& actionName) const;
    bool isActionJustReleased(const std::string& actionName) const;
    float getActionAnalog1D(const std::string& actionName) const;
    glm::vec2 getActionAnalog2D(const std::string& actionName) const;
    float getActionDuration(const std::string& actionName) const;
    
    // Raw input queries (bypass action system)
    bool isKeyDown(int scancode) const;
    bool isKeyPressed(int scancode) const;
    bool isKeyReleased(int scancode) const;
    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonReleased(int button) const;
    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseWorldPosition() const;
    glm::vec2 getMouseDelta() const;
    glm::vec2 getMouseWheelDelta() const;
    
    // Input callbacks for custom handling
    using InputCallback = std::function<void(const std::string&, const InputActionState&)>;
    void registerActionCallback(const std::string& actionName, InputCallback callback);
    void unregisterActionCallback(const std::string& actionName);
    
    // Configuration
    void loadInputConfig(const std::string& configFile);
    void saveInputConfig(const std::string& configFile);
    void resetToDefaults();
    
    // State queries
    bool shouldQuit() const;
    bool isInputConsumed() const { return inputConsumed; }
    void setInputConsumed(bool consumed) { inputConsumed = consumed; }
    
    // Window event handling
    bool hasWindowResizeEvent(int& width, int& height) const;
    
    // Debug and introspection
    std::vector<std::string> getActiveContexts() const;
    std::vector<std::string> getRegisteredActions() const;
    std::vector<InputBinding> getActionBindings(const std::string& actionName) const;
    void printInputState() const;

private:
    // Core data
    flecs::world* world = nullptr;
    SDL_Window* window = nullptr;
    flecs::entity inputEntity;
    
    // Service dependencies (cached references)
    CameraService* cameraService = nullptr;
    
    // Context management
    std::unordered_map<std::string, InputContextDefinition> contexts;
    std::vector<std::string> contextStack;
    std::string activeContextName = "default";
    
    // Action system
    std::unordered_map<std::string, InputActionDefinition> actions;
    std::unordered_map<std::string, InputActionState> actionStates;
    std::unordered_map<std::string, InputCallback> actionCallbacks;
    
    // State tracking
    bool initialized = false;
    bool inputConsumed = false;
    float deltaTime = 0.0f;
    
    // Window event state
    bool hasWindowResize = false;
    int windowResizeWidth = 0;
    int windowResizeHeight = 0;
    
    // Direct input state (not ECS components)
    struct KeyboardState {
        static constexpr size_t MAX_KEYS = 512;
        bool keys[MAX_KEYS] = {false};
        bool keysPressed[MAX_KEYS] = {false};
        bool keysReleased[MAX_KEYS] = {false};
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
    } keyboardState;
    
    struct MouseState {
        static constexpr size_t MAX_BUTTONS = 8;
        bool buttons[MAX_BUTTONS] = {false};
        bool buttonsPressed[MAX_BUTTONS] = {false};
        bool buttonsReleased[MAX_BUTTONS] = {false};
        glm::vec2 position{0.0f};
        glm::vec2 delta{0.0f};
        glm::vec2 wheelDelta{0.0f};
    } mouseState;
    
    // Internal methods
    void updateActionStates();
    void synchronizeToECSComponents();
    void evaluateBinding(const InputBinding& binding, const std::string& actionName, InputActionState& state);
    bool isBindingActive(const InputBinding& binding) const;
    float getBindingAnalogValue(const InputBinding& binding) const;
    bool checkModifiers(const InputBinding& binding) const;
    
    void createDefaultContexts();
    void createDefaultActions();
    
    // SDL event handling
    void handleKeyboardEvent(const SDL_Event& event);
    void handleMouseButtonEvent(const SDL_Event& event);
    void handleMouseMotionEvent(const SDL_Event& event);
    void handleMouseWheelEvent(const SDL_Event& event);
    void handleWindowEvent(const SDL_Event& event);
    
    // ECS integration helpers
    KeyboardInput* getKeyboardInput() const;
    MouseInput* getMouseInput() const;
    InputState* getInputState() const;
    InputEvents* getInputEvents() const;
};

// Convenience functions for global access
namespace Input {
    InputService& getService();
    
    // Quick action queries
    bool isActive(const std::string& actionName);
    bool justPressed(const std::string& actionName);
    bool justReleased(const std::string& actionName);
    float getAnalog1D(const std::string& actionName);
    glm::vec2 getAnalog2D(const std::string& actionName);
    
    // Context switching shortcuts
    void pushContext(const std::string& contextName);
    void popContext();
    void setContext(const std::string& contextName, bool active);
}