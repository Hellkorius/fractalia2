#pragma once

#include "../core/world_manager.h"
#include "../systems/input_system.h"
#include "../components/component.h"
#include <SDL3/SDL.h>
#include <flecs.h>
#include <memory>

/**
 * @brief Self-contained input module that encapsulates all input-related systems
 * 
 * This module handles:
 * - SDL event processing
 * - Input state management  
 * - Input event generation
 * - Integration with input service
 * - Proper phase ordering for input processing
 */
class InputModule : public ECSModule {
public:
    explicit InputModule(SDL_Window* window = nullptr);
    ~InputModule() override;

    // ECSModule interface
    bool initialize(flecs::world& world) override;
    void shutdown() override;
    void update(float deltaTime) override;
    const std::string& getName() const override;

    // Input module specific interface
    void setWindow(SDL_Window* window);
    SDL_Window* getWindow() const { return window_; }

    // Input queries - convenience methods that delegate to InputQuery namespace
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
    
    bool shouldQuit() const;

    // Get the input entity for direct component access
    flecs::entity getInputEntity() const { return inputEntity_; }

private:
    static const std::string MODULE_NAME;
    
    SDL_Window* window_;
    flecs::world* world_;
    flecs::entity inputEntity_;
    
    // System entities for proper cleanup
    flecs::entity inputProcessingSystem_;
    
    void registerInputSystems();
    void setupInputPhases();
    void createInputEntity();
    void cleanupSystems();
};

// Convenience function to get the input module from WorldManager
namespace InputModuleAccess {
    /**
     * @brief Get the input module instance from the world manager
     * @param world The Flecs world instance
     * @return Shared pointer to the input module, or nullptr if not loaded
     */
    std::shared_ptr<InputModule> getInputModule(flecs::world& world);
    
    /**
     * @brief Quick access to input queries without needing module reference
     * @param world The Flecs world instance  
     * @return True if the specified key is currently down
     */
    bool isKeyDown(flecs::world& world, int scancode);
    bool isMouseButtonDown(flecs::world& world, int button);
    glm::vec2 getMousePosition(flecs::world& world);
    bool shouldQuit(flecs::world& world);
}