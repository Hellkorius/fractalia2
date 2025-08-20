#include "input_service.h"
#include "../systems/input_system.h"
#include <iostream>
#include <algorithm>
#include <fstream>

InputService::InputService() {
}

InputService::~InputService() {
    cleanup();
}

bool InputService::initialize(flecs::world& world, SDL_Window* window) {
    if (initialized) {
        return true;
    }
    
    this->world = &world;
    this->window = window;
    
    // Create or get the input entity from InputManager
    inputEntity = InputManager::createInputEntity(world);
    if (!inputEntity.is_valid()) {
        std::cerr << "InputService: Failed to create input entity" << std::endl;
        return false;
    }
    
    // Set window reference in InputManager
    InputManager::setWindow(window);
    
    // Create default contexts and actions
    createDefaultContexts();
    createDefaultActions();
    
    initialized = true;
    return true;
}

void InputService::cleanup() {
    if (!initialized) {
        return;
    }
    
    contexts.clear();
    contextStack.clear();
    actions.clear();
    actionStates.clear();
    actionCallbacks.clear();
    
    world = nullptr;
    window = nullptr;
    inputEntity = flecs::entity::null();
    initialized = false;
}

void InputService::processFrame(float deltaTime) {
    if (!initialized) {
        return;
    }
    
    this->deltaTime = deltaTime;
    
    // Process SDL events directly (removed legacy InputManager call)
    
    // Update action states based on current input
    updateActionStates();
    
    // Execute callbacks for active actions
    for (const auto& [actionName, state] : actionStates) {
        auto callbackIt = actionCallbacks.find(actionName);
        if (callbackIt != actionCallbacks.end() && 
            (state.justPressed || state.justReleased || 
             (state.digitalValue && state.type == InputActionType::DIGITAL) ||
             (std::abs(state.analogValue1D) > 0.01f && state.type == InputActionType::ANALOG_1D) ||
             (glm::length(state.analogValue2D) > 0.01f && state.type == InputActionType::ANALOG_2D))) {
            callbackIt->second(actionName, state);
        }
    }
}

void InputService::processSDLEvents() {
    if (!initialized) {
        return;
    }
    
    // Clear frame-based input states from previous frame (updated)
    std::fill(keyboardState.keysPressed, keyboardState.keysPressed + KeyboardState::MAX_KEYS, false);
    std::fill(keyboardState.keysReleased, keyboardState.keysReleased + KeyboardState::MAX_KEYS, false);
    std::fill(mouseState.buttonsPressed, mouseState.buttonsPressed + MouseState::MAX_BUTTONS, false);
    std::fill(mouseState.buttonsReleased, mouseState.buttonsReleased + MouseState::MAX_BUTTONS, false);
    mouseState.delta = glm::vec2(0.0f);
    mouseState.wheelDelta = glm::vec2(0.0f);
    
    // Clear window events from previous frame
    hasWindowResize = false;
    
    // Process SDL events directly
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                {
                    auto* appState = world->get_mut<ApplicationState>();
                    if (appState) {
                        appState->requestQuit = true;
                    }
                }
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
                
            default:
                break;
        }
    }
    
    // Action states will be updated in processFrame() - don't double-update
}

void InputService::registerContext(const std::string& name, int priority) {
    InputContextDefinition context;
    context.name = name;
    context.priority = priority;
    context.active = false;
    contexts[name] = context;
}

void InputService::setContextActive(const std::string& contextName, bool active) {
    auto it = contexts.find(contextName);
    if (it != contexts.end()) {
        it->second.active = active;
        if (active) {
            activeContextName = contextName;
        }
    }
}

void InputService::pushContext(const std::string& contextName) {
    if (contexts.find(contextName) != contexts.end()) {
        contextStack.push_back(activeContextName);
        setContextActive(contextName, true);
    }
}

void InputService::popContext() {
    if (!contextStack.empty()) {
        std::string previousContext = contextStack.back();
        contextStack.pop_back();
        setContextActive(previousContext, true);
    }
}

std::string InputService::getCurrentContext() const {
    return activeContextName;
}

void InputService::registerAction(const InputActionDefinition& actionDef) {
    actions[actionDef.name] = actionDef;
    
    // Initialize action state
    InputActionState state;
    state.type = actionDef.type;
    actionStates[actionDef.name] = state;
    
    // Add default bindings to default context
    for (const auto& binding : actionDef.defaultBindings) {
        bindAction("default", actionDef.name, binding);
    }
}

void InputService::bindAction(const std::string& contextName, const std::string& actionName, const InputBinding& binding) {
    auto contextIt = contexts.find(contextName);
    if (contextIt != contexts.end()) {
        contextIt->second.actionBindings[actionName].push_back(binding);
    }
}

void InputService::unbindAction(const std::string& contextName, const std::string& actionName) {
    auto contextIt = contexts.find(contextName);
    if (contextIt != contexts.end()) {
        contextIt->second.actionBindings.erase(actionName);
    }
}

void InputService::clearActionBindings(const std::string& actionName) {
    for (auto& [contextName, context] : contexts) {
        context.actionBindings.erase(actionName);
    }
}

bool InputService::isActionActive(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    if (it != actionStates.end()) {
        return it->second.digitalValue || 
               std::abs(it->second.analogValue1D) > 0.01f ||
               glm::length(it->second.analogValue2D) > 0.01f;
    }
    return false;
}

bool InputService::isActionJustPressed(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() && it->second.justPressed;
}

bool InputService::isActionJustReleased(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() && it->second.justReleased;
}

float InputService::getActionAnalog1D(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? it->second.analogValue1D : 0.0f;
}

glm::vec2 InputService::getActionAnalog2D(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? it->second.analogValue2D : glm::vec2(0.0f);
}

float InputService::getActionDuration(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? it->second.duration : 0.0f;
}

// Raw input queries - use direct input state
bool InputService::isKeyDown(int scancode) const {
    if (!initialized || scancode < 0 || scancode >= static_cast<int>(KeyboardState::MAX_KEYS)) {
        return false;
    }
    return keyboardState.keys[scancode];
}

bool InputService::isKeyPressed(int scancode) const {
    if (!initialized || scancode < 0 || scancode >= static_cast<int>(KeyboardState::MAX_KEYS)) {
        return false;
    }
    return keyboardState.keysPressed[scancode];
}

bool InputService::isKeyReleased(int scancode) const {
    if (!initialized || scancode < 0 || scancode >= static_cast<int>(KeyboardState::MAX_KEYS)) {
        return false;
    }
    return keyboardState.keysReleased[scancode];
}

bool InputService::isMouseButtonDown(int button) const {
    if (!initialized || button <= 0 || (button - 1) >= static_cast<int>(MouseState::MAX_BUTTONS)) {
        return false;
    }
    return mouseState.buttons[button - 1]; // Convert SDL 1-based to 0-based array index
}

bool InputService::isMouseButtonPressed(int button) const {
    if (!initialized || button <= 0 || (button - 1) >= static_cast<int>(MouseState::MAX_BUTTONS)) {
        return false;
    }
    return mouseState.buttonsPressed[button - 1]; // Convert SDL 1-based to 0-based array index
}

bool InputService::isMouseButtonReleased(int button) const {
    if (!initialized || button <= 0 || (button - 1) >= static_cast<int>(MouseState::MAX_BUTTONS)) {
        return false;
    }
    return mouseState.buttonsReleased[button - 1]; // Convert SDL 1-based to 0-based array index
}

glm::vec2 InputService::getMousePosition() const {
    return initialized ? mouseState.position : glm::vec2(0.0f);
}

glm::vec2 InputService::getMouseWorldPosition() const {
    // TODO: Convert screen position to world position using camera service
    return initialized ? mouseState.position : glm::vec2(0.0f);
}

glm::vec2 InputService::getMouseDelta() const {
    return initialized ? mouseState.delta : glm::vec2(0.0f);
}

glm::vec2 InputService::getMouseWheelDelta() const {
    return initialized ? mouseState.wheelDelta : glm::vec2(0.0f);
}

void InputService::registerActionCallback(const std::string& actionName, InputCallback callback) {
    actionCallbacks[actionName] = callback;
}

void InputService::unregisterActionCallback(const std::string& actionName) {
    actionCallbacks.erase(actionName);
}

void InputService::loadInputConfig(const std::string& configFile) {
    // TODO: Implement configuration loading from file
    // For now, just use defaults
    resetToDefaults();
}

void InputService::saveInputConfig(const std::string& configFile) {
    // TODO: Implement configuration saving to file
    std::cout << "InputService: Saving config to " << configFile << " (not implemented)" << std::endl;
}

void InputService::resetToDefaults() {
    contexts.clear();
    createDefaultContexts();
    
    // Rebind all registered actions with their default bindings
    for (const auto& [actionName, actionDef] : actions) {
        clearActionBindings(actionName);
        for (const auto& binding : actionDef.defaultBindings) {
            bindAction("default", actionName, binding);
        }
    }
}

bool InputService::shouldQuit() const {
    return initialized ? InputQuery::shouldQuit(*world) : false;
}

std::vector<std::string> InputService::getActiveContexts() const {
    std::vector<std::string> activeContexts;
    for (const auto& [name, context] : contexts) {
        if (context.active) {
            activeContexts.push_back(name);
        }
    }
    return activeContexts;
}

std::vector<std::string> InputService::getRegisteredActions() const {
    std::vector<std::string> actionNames;
    for (const auto& [name, action] : actions) {
        actionNames.push_back(name);
    }
    return actionNames;
}

std::vector<InputBinding> InputService::getActionBindings(const std::string& actionName) const {
    std::vector<InputBinding> allBindings;
    
    // Collect bindings from all active contexts
    for (const auto& [contextName, context] : contexts) {
        if (context.active) {
            auto bindingIt = context.actionBindings.find(actionName);
            if (bindingIt != context.actionBindings.end()) {
                allBindings.insert(allBindings.end(), 
                                 bindingIt->second.begin(), 
                                 bindingIt->second.end());
            }
        }
    }
    
    return allBindings;
}

void InputService::printInputState() const {
    std::cout << "=== Input Service State ===" << std::endl;
    std::cout << "Active Context: " << activeContextName << std::endl;
    std::cout << "Context Stack Size: " << contextStack.size() << std::endl;
    std::cout << "Registered Actions: " << actions.size() << std::endl;
    std::cout << "Active Actions:" << std::endl;
    
    for (const auto& [actionName, state] : actionStates) {
        if (isActionActive(actionName)) {
            std::cout << "  " << actionName << ": ";
            if (state.type == InputActionType::DIGITAL) {
                std::cout << "DIGITAL (" << (state.digitalValue ? "ON" : "OFF") << ")";
            } else if (state.type == InputActionType::ANALOG_1D) {
                std::cout << "ANALOG_1D (" << state.analogValue1D << ")";
            } else if (state.type == InputActionType::ANALOG_2D) {
                std::cout << "ANALOG_2D (" << state.analogValue2D.x << ", " << state.analogValue2D.y << ")";
            }
            std::cout << " Duration: " << state.duration << "s" << std::endl;
        }
    }
}

// Private methods

void InputService::updateActionStates() {
    if (!initialized) {
        return;
    }
    
    // Clear frame-based states (justPressed/justReleased flags are cleared here)
    // Note: digitalValue states are updated immediately in event handlers for tight response
    for (auto& [actionName, state] : actionStates) {
        state.justPressed = false;
        state.justReleased = false;
    }
    
    // Update each registered action
    for (auto& [actionName, state] : actionStates) {
        bool wasActive = state.digitalValue || 
                        std::abs(state.analogValue1D) > 0.01f ||
                        glm::length(state.analogValue2D) > 0.01f;
        
        // Reset values
        state.digitalValue = false;
        state.analogValue1D = 0.0f;
        state.analogValue2D = glm::vec2(0.0f);
        
        // Evaluate bindings from active contexts (highest priority first)
        std::vector<std::pair<int, InputContextDefinition*>> sortedContexts;
        for (auto& [contextName, context] : contexts) {
            if (context.active) {
                sortedContexts.emplace_back(context.priority, &context);
            }
        }
        
        std::sort(sortedContexts.begin(), sortedContexts.end(),
                 [](const auto& a, const auto& b) { return a.first > b.first; });
        
        for (const auto& [priority, context] : sortedContexts) {
            auto bindingIt = context->actionBindings.find(actionName);
            if (bindingIt != context->actionBindings.end()) {
                for (const auto& binding : bindingIt->second) {
                    evaluateBinding(binding, actionName, state);
                }
            }
        }
        
        // Update duration and transition states
        bool isActive = state.digitalValue || 
                       std::abs(state.analogValue1D) > 0.01f ||
                       glm::length(state.analogValue2D) > 0.01f;
        
        if (isActive) {
            if (!wasActive) {
                state.justPressed = true;
                state.duration = 0.0f;
            } else {
                state.duration += deltaTime;
            }
        } else {
            if (wasActive) {
                state.justReleased = true;
            }
            state.duration = 0.0f;
        }
    }
}

void InputService::evaluateBinding(const InputBinding& binding, const std::string& actionName, InputActionState& state) {
    if (!checkModifiers(binding)) {
        return;
    }
    
    auto actionIt = actions.find(actionName);
    if (actionIt == actions.end()) {
        return;
    }
    
    const auto& actionDef = actionIt->second;
    
    switch (binding.inputType) {
        case InputBinding::InputType::KEYBOARD_KEY:
            if (actionDef.type == InputActionType::DIGITAL) {
                state.digitalValue = state.digitalValue || isKeyDown(binding.keycode);
            }
            break;
            
        case InputBinding::InputType::MOUSE_BUTTON:
            if (actionDef.type == InputActionType::DIGITAL) {
                state.digitalValue = state.digitalValue || isMouseButtonDown(binding.mouseButton);
            }
            break;
            
        case InputBinding::InputType::MOUSE_AXIS_X:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = getMouseDelta().x * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue1D += value;
                }
            } else if (actionDef.type == InputActionType::ANALOG_2D) {
                float value = getMouseDelta().x * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue2D.x += value;
                }
            }
            break;
            
        case InputBinding::InputType::MOUSE_AXIS_Y:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = getMouseDelta().y * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue1D += value;
                }
            } else if (actionDef.type == InputActionType::ANALOG_2D) {
                float value = getMouseDelta().y * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue2D.y += value;
                }
            }
            break;
            
        case InputBinding::InputType::MOUSE_WHEEL_X:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = getMouseWheelDelta().x * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                state.analogValue1D += value;
            }
            break;
            
        case InputBinding::InputType::MOUSE_WHEEL_Y:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = getMouseWheelDelta().y * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                state.analogValue1D += value;
            }
            break;
    }
}

bool InputService::checkModifiers(const InputBinding& binding) const {
    if (!initialized) {
        return false;
    }
    
    // Use direct input state instead of ECS components
    if (binding.requiresShift && !keyboardState.shift) return false;
    if (binding.requiresCtrl && !keyboardState.ctrl) return false;
    if (binding.requiresAlt && !keyboardState.alt) return false;
    
    return true;
}

void InputService::createDefaultContexts() {
    registerContext("default", 0);
    registerContext("gameplay", 10);
    registerContext("ui", 20);
    registerContext("debug", 30);
    
    setContextActive("default", true);
}

void InputService::createDefaultActions() {
    // Movement actions
    InputActionDefinition moveLeft;
    moveLeft.name = "move_left";
    moveLeft.type = InputActionType::DIGITAL;
    moveLeft.description = "Move camera left";
    moveLeft.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_A);
    moveLeft.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_LEFT);
    registerAction(moveLeft);
    
    InputActionDefinition moveRight;
    moveRight.name = "move_right";
    moveRight.type = InputActionType::DIGITAL;
    moveRight.description = "Move camera right";
    moveRight.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_D);
    moveRight.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_RIGHT);
    registerAction(moveRight);
    
    InputActionDefinition moveUp;
    moveUp.name = "move_up";
    moveUp.type = InputActionType::DIGITAL;
    moveUp.description = "Move camera up";
    moveUp.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_W);
    moveUp.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_UP);
    registerAction(moveUp);
    
    InputActionDefinition moveDown;
    moveDown.name = "move_down";
    moveDown.type = InputActionType::DIGITAL;
    moveDown.description = "Move camera down";
    moveDown.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_S);
    moveDown.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_DOWN);
    registerAction(moveDown);
    
    // Mouse look
    InputActionDefinition mouseLook;
    mouseLook.name = "mouse_look";
    mouseLook.type = InputActionType::ANALOG_2D;
    mouseLook.description = "Mouse look/camera rotation";
    mouseLook.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_AXIS_X, 0);
    mouseLook.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_AXIS_Y, 0);
    registerAction(mouseLook);
    
    // Zoom
    InputActionDefinition zoomIn;
    zoomIn.name = "zoom_in";
    zoomIn.type = InputActionType::ANALOG_1D;
    zoomIn.description = "Zoom camera in";
    zoomIn.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_WHEEL_Y, 0);
    registerAction(zoomIn);
    
    // Primary action
    InputActionDefinition primaryAction;
    primaryAction.name = "primary_action";
    primaryAction.type = InputActionType::DIGITAL;
    primaryAction.description = "Primary mouse button";
    primaryAction.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_BUTTON, SDL_BUTTON_LEFT);
    registerAction(primaryAction);
    
    // Secondary action
    InputActionDefinition secondaryAction;
    secondaryAction.name = "secondary_action";
    secondaryAction.type = InputActionType::DIGITAL;
    secondaryAction.description = "Secondary mouse button";
    secondaryAction.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_BUTTON, SDL_BUTTON_RIGHT);
    registerAction(secondaryAction);
    
    // Quit
    InputActionDefinition quit;
    quit.name = "quit";
    quit.type = InputActionType::DIGITAL;
    quit.description = "Quit application";
    quit.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_ESCAPE);
    registerAction(quit);
}

// Helper methods to get ECS components
KeyboardInput* InputService::getKeyboardInput() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<KeyboardInput>();
}

MouseInput* InputService::getMouseInput() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<MouseInput>();
}

InputState* InputService::getInputState() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<InputState>();
}

InputEvents* InputService::getInputEvents() const {
    if (!initialized || !inputEntity.is_valid()) {
        return nullptr;
    }
    return inputEntity.get_mut<InputEvents>();
}

bool InputService::hasWindowResizeEvent(int& width, int& height) const {
    if (hasWindowResize) {
        width = windowResizeWidth;
        height = windowResizeHeight;
        return true;
    }
    return false;
}

// SDL event handling implementations
void InputService::handleKeyboardEvent(const SDL_Event& event) {
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

void InputService::handleMouseButtonEvent(const SDL_Event& event) {
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

void InputService::handleMouseMotionEvent(const SDL_Event& event) {
    mouseState.position.x = static_cast<float>(event.motion.x);
    mouseState.position.y = static_cast<float>(event.motion.y);
    mouseState.delta.x = static_cast<float>(event.motion.xrel);
    mouseState.delta.y = static_cast<float>(event.motion.yrel);
}

void InputService::handleMouseWheelEvent(const SDL_Event& event) {
    mouseState.wheelDelta.x = static_cast<float>(event.wheel.x);
    mouseState.wheelDelta.y = static_cast<float>(event.wheel.y);
}

void InputService::handleWindowEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        hasWindowResize = true;
        windowResizeWidth = event.window.data1;
        windowResizeHeight = event.window.data2;
    }
}


// Convenience namespace implementation
namespace Input {
    InputService& getService() {
        return ServiceLocator::instance().requireService<InputService>();
    }
    
    bool isActive(const std::string& actionName) {
        return getService().isActionActive(actionName);
    }
    
    bool justPressed(const std::string& actionName) {
        return getService().isActionJustPressed(actionName);
    }
    
    bool justReleased(const std::string& actionName) {
        return getService().isActionJustReleased(actionName);
    }
    
    float getAnalog1D(const std::string& actionName) {
        return getService().getActionAnalog1D(actionName);
    }
    
    glm::vec2 getAnalog2D(const std::string& actionName) {
        return getService().getActionAnalog2D(actionName);
    }
    
    void pushContext(const std::string& contextName) {
        getService().pushContext(contextName);
    }
    
    void popContext() {
        getService().popContext();
    }
    
    void setContext(const std::string& contextName, bool active) {
        getService().setContextActive(contextName, active);
    }
}