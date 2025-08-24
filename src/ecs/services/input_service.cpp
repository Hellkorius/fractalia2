#include "input_service.h"
#include "camera_service.h"
#include <iostream>
#include <algorithm>
#include <fstream>

InputService::InputService() {
    // Initialize modular components
    eventProcessor = std::make_unique<InputEventProcessor>();
    actionSystem = std::make_unique<InputActionSystem>();
    contextManager = std::make_unique<InputContextManager>();
    configManager = std::make_unique<InputConfigManager>();
    ecsBridge = std::make_unique<InputECSBridge>();
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
    
    // Initialize modular components in correct order
    if (!eventProcessor->initialize(window)) {
        std::cerr << "InputService: Failed to initialize InputEventProcessor" << std::endl;
        return false;
    }
    
    if (!contextManager->initialize()) {
        std::cerr << "InputService: Failed to initialize InputContextManager" << std::endl;
        return false;
    }
    
    if (!actionSystem->initialize(contextManager.get())) {
        std::cerr << "InputService: Failed to initialize InputActionSystem" << std::endl;
        return false;
    }
    
    if (!configManager->initialize(actionSystem.get(), contextManager.get())) {
        std::cerr << "InputService: Failed to initialize InputConfigManager" << std::endl;
        return false;
    }
    
    if (!ecsBridge->initialize(world)) {
        std::cerr << "InputService: Failed to initialize InputECSBridge" << std::endl;
        return false;
    }
    
    // Setup defaults
    configManager->resetToDefaults();
    
    // Cache service dependencies
    if (ServiceLocator::instance().hasService<CameraService>()) {
        cameraService = &ServiceLocator::instance().requireService<CameraService>();
    }
    
    initialized = true;
    return true;
}

void InputService::cleanup() {
    if (!initialized) {
        return;
    }
    
    // Clean up modular components in reverse order
    if (ecsBridge) ecsBridge->cleanup();
    if (configManager) configManager->cleanup();
    if (actionSystem) actionSystem->cleanup();
    if (contextManager) contextManager->cleanup();
    if (eventProcessor) eventProcessor->cleanup();
    
    world = nullptr;
    window = nullptr;
    cameraService = nullptr;
    initialized = false;
}

void InputService::processFrame(float deltaTime) {
    if (!initialized) {
        return;
    }
    
    // Update action states using modular components
    actionSystem->updateActionStates(
        eventProcessor->getKeyboardState(),
        eventProcessor->getMouseState(),
        *contextManager,
        deltaTime
    );
    
    // Execute action callbacks
    actionSystem->executeCallbacks();
    
    // Synchronize to ECS components
    ecsBridge->synchronizeToECSComponents(
        eventProcessor->getKeyboardState(),
        eventProcessor->getMouseState(),
        deltaTime
    );
}

void InputService::processSDLEvents() {
    if (!initialized || !eventProcessor) {
        return;
    }
    
    // Delegate to InputEventProcessor
    eventProcessor->processSDLEvents();
    
    // Handle quit events by updating ECS component
    if (eventProcessor->shouldQuit()) {
        auto* inputState = ecsBridge->getInputState();
        if (inputState) {
            inputState->quit = true;
        }
    }
}

// Context management - delegate to InputContextManager
void InputService::registerContext(const std::string& name, int priority) {
    if (contextManager) {
        contextManager->registerContext(name, priority);
    }
}

void InputService::setContextActive(const std::string& contextName, bool active) {
    if (contextManager) {
        contextManager->setContextActive(contextName, active);
    }
}

void InputService::pushContext(const std::string& contextName) {
    if (contextManager) {
        contextManager->pushContext(contextName);
    }
}

void InputService::popContext() {
    if (contextManager) {
        contextManager->popContext();
    }
}

std::string InputService::getCurrentContext() const {
    return contextManager ? contextManager->getCurrentContext() : "default";
}

// Action system - delegate to InputActionSystem  
void InputService::registerAction(const InputActionDefinition& actionDef) {
    if (actionSystem) {
        // ActionSystem handles its own binding to contexts now - clean separation
        actionSystem->registerAction(actionDef);
    }
}

void InputService::bindAction(const std::string& contextName, const std::string& actionName, const InputBinding& binding) {
    if (contextManager) {
        contextManager->bindAction(contextName, actionName, binding);
    }
}

void InputService::unbindAction(const std::string& contextName, const std::string& actionName) {
    if (contextManager) {
        contextManager->unbindAction(contextName, actionName);
    }
}

void InputService::clearActionBindings(const std::string& actionName) {
    if (contextManager) {
        contextManager->clearActionBindings(actionName);
    }
}

// Action queries - delegate to InputActionSystem
bool InputService::isActionActive(const std::string& actionName) const {
    return actionSystem ? actionSystem->isActionActive(actionName) : false;
}

bool InputService::isActionJustPressed(const std::string& actionName) const {
    return actionSystem ? actionSystem->isActionJustPressed(actionName) : false;
}

bool InputService::isActionJustReleased(const std::string& actionName) const {
    return actionSystem ? actionSystem->isActionJustReleased(actionName) : false;
}

float InputService::getActionAnalog1D(const std::string& actionName) const {
    return actionSystem ? actionSystem->getActionAnalog1D(actionName) : 0.0f;
}

glm::vec2 InputService::getActionAnalog2D(const std::string& actionName) const {
    return actionSystem ? actionSystem->getActionAnalog2D(actionName) : glm::vec2(0.0f);
}

float InputService::getActionDuration(const std::string& actionName) const {
    return actionSystem ? actionSystem->getActionDuration(actionName) : 0.0f;
}

// Raw input queries - delegate to InputEventProcessor
bool InputService::isKeyDown(int scancode) const {
    return eventProcessor ? eventProcessor->isKeyDown(scancode) : false;
}

bool InputService::isKeyPressed(int scancode) const {
    return eventProcessor ? eventProcessor->isKeyPressed(scancode) : false;
}

bool InputService::isKeyReleased(int scancode) const {
    return eventProcessor ? eventProcessor->isKeyReleased(scancode) : false;
}

bool InputService::isMouseButtonDown(int button) const {
    return eventProcessor ? eventProcessor->isMouseButtonDown(button) : false;
}

bool InputService::isMouseButtonPressed(int button) const {
    return eventProcessor ? eventProcessor->isMouseButtonPressed(button) : false;
}

bool InputService::isMouseButtonReleased(int button) const {
    return eventProcessor ? eventProcessor->isMouseButtonReleased(button) : false;
}

glm::vec2 InputService::getMousePosition() const {
    return eventProcessor ? eventProcessor->getMousePosition() : glm::vec2(0.0f);
}

glm::vec2 InputService::getMouseWorldPosition() const {
    if (!eventProcessor || !ecsBridge || !cameraService || !window) {
        return glm::vec2(0.0f);
    }
    
    return ecsBridge->getMouseWorldPosition(eventProcessor->getMouseState(), cameraService, window);
}

glm::vec2 InputService::getMouseDelta() const {
    return eventProcessor ? eventProcessor->getMouseDelta() : glm::vec2(0.0f);
}

glm::vec2 InputService::getMouseWheelDelta() const {
    return eventProcessor ? eventProcessor->getMouseWheelDelta() : glm::vec2(0.0f);
}

// Input callbacks - delegate to InputActionSystem
void InputService::registerActionCallback(const std::string& actionName, InputCallback callback) {
    if (actionSystem) {
        actionSystem->registerActionCallback(actionName, callback);
    }
}

void InputService::unregisterActionCallback(const std::string& actionName) {
    if (actionSystem) {
        actionSystem->unregisterActionCallback(actionName);
    }
}

// Configuration - delegate to InputConfigManager
void InputService::loadInputConfig(const std::string& configFile) {
    if (configManager) {
        configManager->loadInputConfig(configFile);
    }
}

void InputService::saveInputConfig(const std::string& configFile) {
    if (configManager) {
        configManager->saveInputConfig(configFile);
    }
}

void InputService::resetToDefaults() {
    if (configManager) {
        configManager->resetToDefaults();
    }
}

// State queries - delegate to InputEventProcessor
bool InputService::shouldQuit() const {
    return eventProcessor ? eventProcessor->shouldQuit() : false;
}

bool InputService::isInputConsumed() const {
    return eventProcessor ? eventProcessor->isInputConsumed() : false;
}

void InputService::setInputConsumed(bool consumed) {
    if (eventProcessor) {
        eventProcessor->setInputConsumed(consumed);
    }
}

// Window event handling - delegate to InputEventProcessor
bool InputService::hasWindowResizeEvent(int& width, int& height) const {
    return eventProcessor ? eventProcessor->hasWindowResizeEvent(width, height) : false;
}

// Mouse control
void InputService::centerMouseCursor() {
    if (!initialized || !window) {
        return;
    }
    
    // Get window size
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    
    // Calculate center position
    float centerX = windowWidth / 2.0f;
    float centerY = windowHeight / 2.0f;
    
    // Warp mouse to center
    SDL_WarpMouseInWindow(window, centerX, centerY);
}

// Debug and introspection
std::vector<std::string> InputService::getActiveContexts() const {
    return contextManager ? contextManager->getActiveContexts() : std::vector<std::string>();
}

std::vector<std::string> InputService::getRegisteredActions() const {
    return actionSystem ? actionSystem->getRegisteredActions() : std::vector<std::string>();
}

std::vector<InputBinding> InputService::getActionBindings(const std::string& actionName) const {
    return contextManager ? contextManager->getActionBindings(actionName) : std::vector<InputBinding>();
}

void InputService::printInputState() const {
    if (actionSystem) {
        actionSystem->printActionStates();
    }
    if (contextManager) {
        contextManager->printContextState();
    }
}

// ECS component access - delegate to InputECSBridge
KeyboardInput* InputService::getKeyboardInput() const {
    return ecsBridge ? ecsBridge->getKeyboardInput() : nullptr;
}

MouseInput* InputService::getMouseInput() const {
    return ecsBridge ? ecsBridge->getMouseInput() : nullptr;
}

InputState* InputService::getInputState() const {
    return ecsBridge ? ecsBridge->getInputState() : nullptr;
}

InputEvents* InputService::getInputEvents() const {
    return ecsBridge ? ecsBridge->getInputEvents() : nullptr;
}