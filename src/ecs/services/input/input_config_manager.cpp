#include "input_config_manager.h"
#include "input_action_system.h"
#include "input_context_manager.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>

InputConfigManager::InputConfigManager() {
}

InputConfigManager::~InputConfigManager() {
    cleanup();
}

bool InputConfigManager::initialize(InputActionSystem* actionSystem, InputContextManager* contextManager) {
    if (initialized) {
        return true;
    }
    
    this->actionSystem = actionSystem;
    this->contextManager = contextManager;
    
    if (!actionSystem || !contextManager) {
        std::cerr << "InputConfigManager: Invalid module references" << std::endl;
        return false;
    }
    
    initialized = true;
    return true;
}

void InputConfigManager::cleanup() {
    if (!initialized) {
        return;
    }
    
    actionSystem = nullptr;
    contextManager = nullptr;
    initialized = false;
}

void InputConfigManager::loadInputConfig(const std::string& configFile) {
    if (!initialized) {
        return;
    }
    
    // TODO: Implement JSON/custom config file loading
    std::cout << "InputConfigManager: Loading config from " << configFile << " (not implemented)" << std::endl;
    
    // For now, just use defaults
    resetToDefaults();
}

void InputConfigManager::saveInputConfig(const std::string& configFile) {
    if (!initialized) {
        return;
    }
    
    // TODO: Implement JSON/custom config file saving
    std::cout << "InputConfigManager: Saving config to " << configFile << " (not implemented)" << std::endl;
}

void InputConfigManager::resetToDefaults() {
    if (!initialized) {
        return;
    }
    
    createDefaultContexts();
    createDefaultActions();
}

void InputConfigManager::createDefaultContexts() {
    if (!contextManager) {
        return;
    }
    
    contextManager->registerContext("default", 0);
    contextManager->registerContext("gameplay", 10);
    contextManager->registerContext("ui", 20);
    contextManager->registerContext("debug", 30);
    
    contextManager->setContextActive("default", true);
}

void InputConfigManager::createDefaultActions() {
    // Don't create default actions here - let services (like ControlService) register their own
    // This prevents conflicts between default actions and service-specific actions
    if (!actionSystem || !contextManager) {
        return;
    }
    
    // Only set up basic system actions that all services need
    setupSystemActions();
}

void InputConfigManager::setupMovementActions() {
    // Movement actions
    InputActionDefinition moveLeft;
    moveLeft.name = "move_left";
    moveLeft.type = InputActionType::DIGITAL;
    moveLeft.description = "Move camera left";
    moveLeft.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_A);
    moveLeft.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_LEFT);
    actionSystem->registerAction(moveLeft);
    
    // Add default bindings to default context
    for (const auto& binding : moveLeft.defaultBindings) {
        contextManager->bindAction("default", moveLeft.name, binding);
    }
    
    InputActionDefinition moveRight;
    moveRight.name = "move_right";
    moveRight.type = InputActionType::DIGITAL;
    moveRight.description = "Move camera right";
    moveRight.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_D);
    moveRight.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_RIGHT);
    actionSystem->registerAction(moveRight);
    
    for (const auto& binding : moveRight.defaultBindings) {
        contextManager->bindAction("default", moveRight.name, binding);
    }
    
    InputActionDefinition moveUp;
    moveUp.name = "move_up";
    moveUp.type = InputActionType::DIGITAL;
    moveUp.description = "Move camera up";
    moveUp.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_W);
    moveUp.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_UP);
    actionSystem->registerAction(moveUp);
    
    for (const auto& binding : moveUp.defaultBindings) {
        contextManager->bindAction("default", moveUp.name, binding);
    }
    
    InputActionDefinition moveDown;
    moveDown.name = "move_down";
    moveDown.type = InputActionType::DIGITAL;
    moveDown.description = "Move camera down";
    moveDown.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_S);
    moveDown.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_DOWN);
    actionSystem->registerAction(moveDown);
    
    for (const auto& binding : moveDown.defaultBindings) {
        contextManager->bindAction("default", moveDown.name, binding);
    }
}

void InputConfigManager::setupMouseActions() {
    // Mouse look
    InputActionDefinition mouseLook;
    mouseLook.name = "mouse_look";
    mouseLook.type = InputActionType::ANALOG_2D;
    mouseLook.description = "Mouse look/camera rotation";
    mouseLook.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_AXIS_X, 0);
    mouseLook.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_AXIS_Y, 0);
    actionSystem->registerAction(mouseLook);
    
    for (const auto& binding : mouseLook.defaultBindings) {
        contextManager->bindAction("default", mouseLook.name, binding);
    }
    
    // Zoom
    InputActionDefinition zoomIn;
    zoomIn.name = "zoom_in";
    zoomIn.type = InputActionType::ANALOG_1D;
    zoomIn.description = "Zoom camera in";
    zoomIn.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_WHEEL_Y, 0);
    actionSystem->registerAction(zoomIn);
    
    for (const auto& binding : zoomIn.defaultBindings) {
        contextManager->bindAction("default", zoomIn.name, binding);
    }
    
    // Primary action
    InputActionDefinition primaryAction;
    primaryAction.name = "primary_action";
    primaryAction.type = InputActionType::DIGITAL;
    primaryAction.description = "Primary mouse button";
    primaryAction.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_BUTTON, SDL_BUTTON_LEFT);
    actionSystem->registerAction(primaryAction);
    
    for (const auto& binding : primaryAction.defaultBindings) {
        contextManager->bindAction("default", primaryAction.name, binding);
    }
    
    // Secondary action
    InputActionDefinition secondaryAction;
    secondaryAction.name = "secondary_action";
    secondaryAction.type = InputActionType::DIGITAL;
    secondaryAction.description = "Secondary mouse button";
    secondaryAction.defaultBindings.emplace_back(InputBinding::InputType::MOUSE_BUTTON, SDL_BUTTON_RIGHT);
    actionSystem->registerAction(secondaryAction);
    
    for (const auto& binding : secondaryAction.defaultBindings) {
        contextManager->bindAction("default", secondaryAction.name, binding);
    }
}

void InputConfigManager::setupSystemActions() {
    // Quit
    InputActionDefinition quit;
    quit.name = "quit";
    quit.type = InputActionType::DIGITAL;
    quit.description = "Quit application";
    quit.defaultBindings.emplace_back(InputBinding::InputType::KEYBOARD_KEY, SDL_SCANCODE_ESCAPE);
    actionSystem->registerAction(quit);
    
    for (const auto& binding : quit.defaultBindings) {
        contextManager->bindAction("default", quit.name, binding);
    }
}