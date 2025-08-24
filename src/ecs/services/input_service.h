#pragma once

#include "../core/service_locator.h"
#include "input/input_types.h"
#include "input/input_action_system.h"
#include "input/input_event_processor.h"
#include "input/input_context_manager.h"
#include "input/input_config_manager.h"
#include "input/input_ecs_bridge.h"
#include <SDL3/SDL.h>
#include <flecs.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>

// Forward declarations
class CameraService;

// Re-export types from modules for backward compatibility
using InputCallback = InputActionSystem::InputCallback;

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
    
    // Context management (delegated to InputContextManager)
    void registerContext(const std::string& name, int priority = 0);
    void setContextActive(const std::string& contextName, bool active);
    void pushContext(const std::string& contextName);
    void popContext();
    std::string getCurrentContext() const;
    
    // Action system (delegated to InputActionSystem)
    void registerAction(const InputActionDefinition& actionDef);
    void bindAction(const std::string& contextName, const std::string& actionName, const InputBinding& binding);
    void unbindAction(const std::string& contextName, const std::string& actionName);
    void clearActionBindings(const std::string& actionName);
    
    // Action queries (delegated to InputActionSystem)
    bool isActionActive(const std::string& actionName) const;
    bool isActionJustPressed(const std::string& actionName) const;
    bool isActionJustReleased(const std::string& actionName) const;
    float getActionAnalog1D(const std::string& actionName) const;
    glm::vec2 getActionAnalog2D(const std::string& actionName) const;
    float getActionDuration(const std::string& actionName) const;
    
    // Raw input queries (delegated to InputEventProcessor)
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
    
    // Input callbacks for custom handling (delegated to InputActionSystem)
    void registerActionCallback(const std::string& actionName, InputCallback callback);
    void unregisterActionCallback(const std::string& actionName);
    
    // Configuration (delegated to InputConfigManager)
    void loadInputConfig(const std::string& configFile);
    void saveInputConfig(const std::string& configFile);
    void resetToDefaults();
    
    // State queries (delegated to InputEventProcessor)
    bool shouldQuit() const;
    bool isInputConsumed() const;
    void setInputConsumed(bool consumed);
    
    // Window event handling (delegated to InputEventProcessor)
    bool hasWindowResizeEvent(int& width, int& height) const;
    
    // Mouse control
    void centerMouseCursor();
    
    // Debug and introspection
    std::vector<std::string> getActiveContexts() const;
    std::vector<std::string> getRegisteredActions() const;
    std::vector<InputBinding> getActionBindings(const std::string& actionName) const;
    void printInputState() const;
    
    // ECS component access (delegated to InputECSBridge)
    KeyboardInput* getKeyboardInput() const;
    MouseInput* getMouseInput() const;
    InputState* getInputState() const;
    InputEvents* getInputEvents() const;

private:
    // Core data
    flecs::world* world = nullptr;
    SDL_Window* window = nullptr;
    
    // Service dependencies (cached references)
    CameraService* cameraService = nullptr;
    
    // Modular components
    std::unique_ptr<InputEventProcessor> eventProcessor;
    std::unique_ptr<InputActionSystem> actionSystem;
    std::unique_ptr<InputContextManager> contextManager;
    std::unique_ptr<InputConfigManager> configManager;
    std::unique_ptr<InputECSBridge> ecsBridge;
    
    // State tracking
    bool initialized = false;
};

