#include "input_action_system.h"
#include "input_context_manager.h"
#include "input_event_processor.h" // For KeyboardState, MouseState
#include <iostream>
#include <algorithm>

InputActionSystem::InputActionSystem() {
}

InputActionSystem::~InputActionSystem() {
    cleanup();
}

bool InputActionSystem::initialize(InputContextManager* contextManager) {
    if (initialized) {
        return true;
    }
    
    this->contextManager = contextManager;
    if (!contextManager) {
        std::cerr << "InputActionSystem: Invalid contextManager reference" << std::endl;
        return false;
    }
    
    initialized = true;
    return true;
}

void InputActionSystem::cleanup() {
    if (!initialized) {
        return;
    }
    
    actions.clear();
    actionStates.clear();
    actionCallbacks.clear();
    initialized = false;
}

void InputActionSystem::registerAction(const InputActionDefinition& actionDef) {
    actions[actionDef.name] = actionDef;
    
    // Initialize action state
    InputActionState& state = actionStates[actionDef.name];
    state.type = actionDef.type;
    state.digitalValue = false;
    state.analogValue1D = 0.0f;
    state.analogValue2D = glm::vec2(0.0f);
    state.justPressed = false;
    state.justReleased = false;
    state.duration = 0.0f;
    
    // Auto-bind default bindings to default context
    if (contextManager) {
        for (const auto& binding : actionDef.defaultBindings) {
            contextManager->bindAction("default", actionDef.name, binding);
        }
    }
}

void InputActionSystem::clearActionBindings(const std::string& actionName) {
    // Action bindings are now managed by InputContextManager
    // This method exists for API compatibility but does nothing
}

void InputActionSystem::updateActionStates(const KeyboardState& keyboardState, 
                                         const MouseState& mouseState,
                                         const InputContextManager& contextManager,
                                         float deltaTime) {
    if (!initialized) {
        return;
    }
    
    // Clear frame-based states
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
        
        // Get bindings from context manager for this action
        auto bindings = contextManager.getActionBindings(actionName);
        for (const auto& binding : bindings) {
            evaluateBinding(binding, actionName, state, keyboardState, mouseState);
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

bool InputActionSystem::isActionActive(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    if (it != actionStates.end()) {
        return it->second.digitalValue || 
               std::abs(it->second.analogValue1D) > 0.01f ||
               glm::length(it->second.analogValue2D) > 0.01f;
    }
    return false;
}

bool InputActionSystem::isActionJustPressed(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() && it->second.justPressed;
}

bool InputActionSystem::isActionJustReleased(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() && it->second.justReleased;
}

float InputActionSystem::getActionAnalog1D(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? it->second.analogValue1D : 0.0f;
}

glm::vec2 InputActionSystem::getActionAnalog2D(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? it->second.analogValue2D : glm::vec2(0.0f);
}

float InputActionSystem::getActionDuration(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? it->second.duration : 0.0f;
}

void InputActionSystem::registerActionCallback(const std::string& actionName, InputCallback callback) {
    actionCallbacks[actionName] = callback;
}

void InputActionSystem::unregisterActionCallback(const std::string& actionName) {
    actionCallbacks.erase(actionName);
}

void InputActionSystem::executeCallbacks() const {
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

std::vector<std::string> InputActionSystem::getRegisteredActions() const {
    std::vector<std::string> actionNames;
    actionNames.reserve(actions.size());
    for (const auto& [name, action] : actions) {
        actionNames.push_back(name);
    }
    return actionNames;
}

const InputActionState* InputActionSystem::getActionState(const std::string& actionName) const {
    auto it = actionStates.find(actionName);
    return it != actionStates.end() ? &it->second : nullptr;
}

void InputActionSystem::printActionStates() const {
    std::cout << "=== Action States ===" << std::endl;
    for (const auto& [actionName, state] : actionStates) {
        if (state.digitalValue || 
            std::abs(state.analogValue1D) > 0.01f ||
            glm::length(state.analogValue2D) > 0.01f) {
            std::cout << actionName << ": ";
            if (state.type == InputActionType::DIGITAL) {
                std::cout << "digital=" << state.digitalValue;
            } else if (state.type == InputActionType::ANALOG_1D) {
                std::cout << "analog1D=" << state.analogValue1D;
            } else if (state.type == InputActionType::ANALOG_2D) {
                std::cout << "analog2D=(" << state.analogValue2D.x << "," << state.analogValue2D.y << ")";
            }
            std::cout << ", duration=" << state.duration << std::endl;
        }
    }
}

void InputActionSystem::evaluateBinding(const InputBinding& binding, 
                                       const std::string& actionName, 
                                       InputActionState& state,
                                       const KeyboardState& keyboardState,
                                       const MouseState& mouseState) {
    if (!checkModifiers(binding, keyboardState)) {
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
                if (binding.keycode >= 0 && binding.keycode < static_cast<int>(KeyboardState::MAX_KEYS)) {
                    state.digitalValue = state.digitalValue || keyboardState.keys[binding.keycode];
                }
            }
            break;
            
        case InputBinding::InputType::MOUSE_BUTTON:
            if (actionDef.type == InputActionType::DIGITAL) {
                if (binding.mouseButton > 0 && (binding.mouseButton - 1) < static_cast<int>(MouseState::MAX_BUTTONS)) {
                    state.digitalValue = state.digitalValue || mouseState.buttons[binding.mouseButton - 1];
                }
            }
            break;
            
        case InputBinding::InputType::MOUSE_AXIS_X:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = mouseState.delta.x * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue1D += value;
                }
            } else if (actionDef.type == InputActionType::ANALOG_2D) {
                float value = mouseState.delta.x * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue2D.x += value;
                }
            }
            break;
            
        case InputBinding::InputType::MOUSE_AXIS_Y:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = mouseState.delta.y * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue1D += value;
                }
            } else if (actionDef.type == InputActionType::ANALOG_2D) {
                float value = mouseState.delta.y * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                if (std::abs(value) > binding.deadzone) {
                    state.analogValue2D.y += value;
                }
            }
            break;
            
        case InputBinding::InputType::MOUSE_WHEEL_X:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = mouseState.wheelDelta.x * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                state.analogValue1D += value;
            }
            break;
            
        case InputBinding::InputType::MOUSE_WHEEL_Y:
            if (actionDef.type == InputActionType::ANALOG_1D) {
                float value = mouseState.wheelDelta.y * binding.sensitivity;
                if (binding.invertAxis) value = -value;
                state.analogValue1D += value;
            }
            break;
    }
}

bool InputActionSystem::isBindingActive(const InputBinding& binding,
                                       const KeyboardState& keyboardState,
                                       const MouseState& mouseState) const {
    switch (binding.inputType) {
        case InputBinding::InputType::KEYBOARD_KEY:
            if (binding.keycode >= 0 && binding.keycode < static_cast<int>(KeyboardState::MAX_KEYS)) {
                return keyboardState.keys[binding.keycode];
            }
            break;
            
        case InputBinding::InputType::MOUSE_BUTTON:
            if (binding.mouseButton > 0 && (binding.mouseButton - 1) < static_cast<int>(MouseState::MAX_BUTTONS)) {
                return mouseState.buttons[binding.mouseButton - 1];
            }
            break;
            
        default:
            // Analog bindings don't have a simple "active" state
            return false;
    }
    return false;
}

float InputActionSystem::getBindingAnalogValue(const InputBinding& binding,
                                              const KeyboardState& keyboardState,
                                              const MouseState& mouseState) const {
    switch (binding.inputType) {
        case InputBinding::InputType::MOUSE_AXIS_X:
            return mouseState.delta.x * binding.sensitivity * (binding.invertAxis ? -1.0f : 1.0f);
        case InputBinding::InputType::MOUSE_AXIS_Y:
            return mouseState.delta.y * binding.sensitivity * (binding.invertAxis ? -1.0f : 1.0f);
        case InputBinding::InputType::MOUSE_WHEEL_X:
            return mouseState.wheelDelta.x * binding.sensitivity * (binding.invertAxis ? -1.0f : 1.0f);
        case InputBinding::InputType::MOUSE_WHEEL_Y:
            return mouseState.wheelDelta.y * binding.sensitivity * (binding.invertAxis ? -1.0f : 1.0f);
        default:
            return 0.0f;
    }
}

bool InputActionSystem::checkModifiers(const InputBinding& binding,
                                      const KeyboardState& keyboardState) const {
    if (binding.requiresShift && !keyboardState.shift) return false;
    if (binding.requiresCtrl && !keyboardState.ctrl) return false;
    if (binding.requiresAlt && !keyboardState.alt) return false;
    return true;
}