#pragma once

#include "input_types.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

// Forward declarations
struct KeyboardState;
struct MouseState;
class InputContextManager;

// Input action system - manages action binding, mapping, and callbacks
class InputActionSystem {
public:
    using InputCallback = std::function<void(const std::string&, const InputActionState&)>;
    
    InputActionSystem();
    ~InputActionSystem();
    
    // Initialization
    bool initialize(InputContextManager* contextManager);
    void cleanup();
    
    // Action registration
    void registerAction(const InputActionDefinition& actionDef);
    void clearActionBindings(const std::string& actionName);
    
    // Action state updates
    void updateActionStates(const KeyboardState& keyboardState, 
                           const MouseState& mouseState,
                           const InputContextManager& contextManager,
                           float deltaTime);
    
    // Action queries
    bool isActionActive(const std::string& actionName) const;
    bool isActionJustPressed(const std::string& actionName) const;
    bool isActionJustReleased(const std::string& actionName) const;
    float getActionAnalog1D(const std::string& actionName) const;
    glm::vec2 getActionAnalog2D(const std::string& actionName) const;
    float getActionDuration(const std::string& actionName) const;
    
    // Callback system
    void registerActionCallback(const std::string& actionName, InputCallback callback);
    void unregisterActionCallback(const std::string& actionName);
    void executeCallbacks() const;
    
    // Debug and introspection
    std::vector<std::string> getRegisteredActions() const;
    const InputActionState* getActionState(const std::string& actionName) const;
    void printActionStates() const;
    
    // Access to action definitions (for configuration)
    const std::unordered_map<std::string, InputActionDefinition>& getActionDefinitions() const { return actions; }
    std::unordered_map<std::string, InputActionDefinition>& getActionDefinitions() { return actions; }

private:
    // Action system data
    std::unordered_map<std::string, InputActionDefinition> actions;
    std::unordered_map<std::string, InputActionState> actionStates;
    std::unordered_map<std::string, InputCallback> actionCallbacks;
    
    // Module references
    InputContextManager* contextManager = nullptr;
    
    bool initialized = false;
    
    // Internal methods
    void evaluateBinding(const InputBinding& binding, 
                        const std::string& actionName, 
                        InputActionState& state,
                        const KeyboardState& keyboardState,
                        const MouseState& mouseState);
    bool isBindingActive(const InputBinding& binding,
                        const KeyboardState& keyboardState,
                        const MouseState& mouseState) const;
    float getBindingAnalogValue(const InputBinding& binding,
                               const KeyboardState& keyboardState,
                               const MouseState& mouseState) const;
    bool checkModifiers(const InputBinding& binding,
                       const KeyboardState& keyboardState) const;
};