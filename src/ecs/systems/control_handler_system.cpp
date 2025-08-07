#include "control_handler_system.hpp"
#include "input_system.hpp"
#include <iostream>

namespace ControlHandler {
    
    void initialize(World& world) {
        std::cout << "\n=== Simple Petal Movement Demo Controls ===" << std::endl;
        std::cout << "ESC: Exit" << std::endl;
        std::cout << "P: Print detailed performance report" << std::endl;
        std::cout << "+/=: Add 1000 more entities" << std::endl;
        std::cout << "-: Show current performance stats" << std::endl;
        std::cout << "Left Click: Create entity with petal movement at mouse position" << std::endl;
        std::cout << "\nCamera Controls:" << std::endl;
        std::cout << "WASD: Move camera" << std::endl;
        std::cout << "Q/E: Move camera up/down" << std::endl;
        std::cout << "Mouse Wheel: Zoom in/out" << std::endl;
        std::cout << "R/T: Rotate camera" << std::endl;
        std::cout << "Shift: Speed boost | Ctrl: Precision mode" << std::endl;
        std::cout << "Space: Reset camera to origin" << std::endl;
        std::cout << "C: Print camera info" << std::endl;
        std::cout << "\nSimple Petal Movement:" << std::endl;
        std::cout << "• Entities emanate from center and return in petal pattern" << std::endl;
        std::cout << "===============================================\n" << std::endl;
    }
    
    void processControls(World& world, bool& running) {
        handleApplicationControls(world, running);
        handleEntityCreation(world);
        handlePerformanceControls(world);
    }
    
    void handleApplicationControls(World& world, bool& running) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Handle quit conditions
        if (InputQuery::shouldQuit(flecsWorld) || 
            InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_ESCAPE)) {
            running = false;
        }
    }
    
    void handleEntityCreation(World& world) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Add more entities (stress test)
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_EQUALS) || 
            InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_KP_PLUS)) {
            std::cout << "Adding 1000 more entities with petal movement..." << std::endl;
            auto newEntities = world.getEntityFactory().createSwarm(1000, glm::vec3(0.0f), 1.5f);
            auto worldStats = world.getStats();
            std::cout << "Total entities now: " << worldStats.memoryStats.activeEntities << std::endl;
        }
        
        // Create entity at mouse position
        if (InputQuery::isMouseButtonPressed(flecsWorld, 0)) { // Left mouse button
            glm::vec2 mouseWorldPos = InputQuery::getMouseWorldPosition(flecsWorld);
            std::cout << "Creating petal movement entity at mouse position: " << mouseWorldPos.x << ", " << mouseWorldPos.y << std::endl;
            
            auto mouseEntity = world.getEntityFactory().createMovingEntity(
                glm::vec3(mouseWorldPos.x, mouseWorldPos.y, 0.0f)
            );
            if (mouseEntity.is_valid()) {
                std::cout << "Created entity with petal movement pattern" << std::endl;
            }
        }
    }
    
    
    void handlePerformanceControls(World& world) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Print performance report
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_P)) {
            Profiler::getInstance().printReport();
        }
        
        // Print current stats
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_MINUS) || 
            InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_KP_MINUS)) {
            auto worldStats = world.getStats();
            float avgFrameTime = Profiler::getInstance().getFrameTime();
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            std::cout << "Current Stats - Entities: " << worldStats.memoryStats.activeEntities 
                      << ", Frame Time: " << avgFrameTime << "ms"
                      << ", FPS: " << fps << std::endl;
        }
    }
    
}