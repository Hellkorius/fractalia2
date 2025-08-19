#include "input_module.h"
#include "../core/service_locator.h"
#include "../systems/input_system.h"
#include <stdexcept>

const std::string InputModule::MODULE_NAME = "InputModule";

InputModule::InputModule(SDL_Window* window)
    : window_(window)
    , world_(nullptr) {
}

InputModule::~InputModule() {
    shutdown();
}

bool InputModule::initialize(flecs::world& world) {
    if (initialized_) {
        return true;
    }

    try {
        world_ = &world;
        
        // Setup input processing phases
        setupInputPhases();
        
        // Create the input entity with all required components
        createInputEntity();
        
        // Register input processing systems with proper phases
        registerInputSystems();
        
        // Set up InputManager context
        if (window_) {
            InputManager::setWindow(window_);
        }
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
        shutdown();
        return false;
    }
}

void InputModule::shutdown() {
    if (!initialized_) {
        return;
    }

    try {
        cleanupSystems();
        
        // Reset input entity
        if (inputEntity_.is_valid()) {
            inputEntity_.destruct();
        }
        
        world_ = nullptr;
        initialized_ = false;
        
    } catch (const std::exception& e) {
        // Log error but continue shutdown
    }
}

void InputModule::update(float deltaTime) {
    if (!initialized_ || !world_) {
        return;
    }

    try {
        // Process SDL events before ECS systems run
        InputManager::processSDLEvents(*world_);
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
    }
}

const std::string& InputModule::getName() const {
    return MODULE_NAME;
}

void InputModule::setWindow(SDL_Window* window) {
    window_ = window;
    if (initialized_) {
        InputManager::setWindow(window);
    }
}

void InputModule::setupInputPhases() {
    // Create input processing phase that runs early in the frame
    auto inputPhase = world_->entity("InputPhase")
        .add(flecs::Phase)
        .depends_on(flecs::PreUpdate);
        
    // Create input events phase that runs after input processing
    auto inputEventsPhase = world_->entity("InputEventsPhase")
        .add(flecs::Phase)
        .depends_on(inputPhase);
}

void InputModule::createInputEntity() {
    // Create the singleton input entity using InputManager
    inputEntity_ = InputManager::createInputEntity(*world_);
    
    if (!inputEntity_.is_valid()) {
        throw std::runtime_error("Failed to create input entity");
    }
}

void InputModule::registerInputSystems() {
    auto inputPhase = world_->entity("InputPhase");
    
    // Register the input processing system to run in the InputPhase
    inputProcessingSystem_ = world_->system<InputState, KeyboardInput, MouseInput, InputEvents>()
        .kind(inputPhase)
        .each(input_processing_system);
        
    if (!inputProcessingSystem_.is_valid()) {
        throw std::runtime_error("Failed to register input processing system");
    }
}

void InputModule::cleanupSystems() {
    if (inputProcessingSystem_.is_valid()) {
        inputProcessingSystem_.destruct();
    }
}

// Input query convenience methods
bool InputModule::isKeyDown(int scancode) const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::isKeyDown(*world_, scancode);
}

bool InputModule::isKeyPressed(int scancode) const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::isKeyPressed(*world_, scancode);
}

bool InputModule::isKeyReleased(int scancode) const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::isKeyReleased(*world_, scancode);
}

bool InputModule::isMouseButtonDown(int button) const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::isMouseButtonDown(*world_, button);
}

bool InputModule::isMouseButtonPressed(int button) const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::isMouseButtonPressed(*world_, button);
}

bool InputModule::isMouseButtonReleased(int button) const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::isMouseButtonReleased(*world_, button);
}

glm::vec2 InputModule::getMousePosition() const {
    if (!initialized_ || !world_) {
        return glm::vec2(0.0f);
    }
    return InputQuery::getMousePosition(*world_);
}

glm::vec2 InputModule::getMouseWorldPosition() const {
    if (!initialized_ || !world_) {
        return glm::vec2(0.0f);
    }
    return InputQuery::getMouseWorldPosition(*world_);
}

glm::vec2 InputModule::getMouseDelta() const {
    if (!initialized_ || !world_) {
        return glm::vec2(0.0f);
    }
    return InputQuery::getMouseDelta(*world_);
}

glm::vec2 InputModule::getMouseWheelDelta() const {
    if (!initialized_ || !world_) {
        return glm::vec2(0.0f);
    }
    return InputQuery::getMouseWheelDelta(*world_);
}

bool InputModule::shouldQuit() const {
    if (!initialized_ || !world_) {
        return false;
    }
    return InputQuery::shouldQuit(*world_);
}

// InputModuleAccess namespace implementation
namespace InputModuleAccess {
    std::shared_ptr<InputModule> getInputModule(flecs::world& world) {
        auto worldManager = ServiceLocator::instance().getService<WorldManager>();
        if (!worldManager) {
            return nullptr;
        }
        
        return worldManager->getModule<InputModule>("InputModule");
    }
    
    bool isKeyDown(flecs::world& world, int scancode) {
        auto module = getInputModule(world);
        return module ? module->isKeyDown(scancode) : false;
    }
    
    bool isMouseButtonDown(flecs::world& world, int button) {
        auto module = getInputModule(world);
        return module ? module->isMouseButtonDown(button) : false;
    }
    
    glm::vec2 getMousePosition(flecs::world& world) {
        auto module = getInputModule(world);
        return module ? module->getMousePosition() : glm::vec2(0.0f);
    }
    
    bool shouldQuit(flecs::world& world) {
        auto module = getInputModule(world);
        return module ? module->shouldQuit() : false;
    }
}