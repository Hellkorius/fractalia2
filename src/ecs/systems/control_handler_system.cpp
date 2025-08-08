#include "control_handler_system.hpp"
#include "input_system.hpp"
#include "../../vulkan_renderer.h"
#include "../gpu_entity_manager.h"
#include <iostream>

namespace ControlHandler {
    
    // Global movement type state (0=Petal, 1=Orbit, 2=Wave)
    int g_currentMovementType = 0;
    bool g_angelModeEnabled = false;
    
    void initialize(World& world) {
        std::cout << "\n=== GPU Compute Movement Demo Controls ===" << std::endl;
        std::cout << "ESC: Exit" << std::endl;
        std::cout << "P: Print detailed performance report" << std::endl;
        std::cout << "+/=: Add 1000 more GPU entities" << std::endl;
        std::cout << "-: Show current GPU performance stats" << std::endl;
        std::cout << "Left Click: Create GPU entity with movement at mouse position" << std::endl;
        std::cout << "0/1/2: Switch movement pattern (0=Petal, 1=Orbit, 2=Wave)" << std::endl;
        std::cout << "CAPS LOCK: Toggle Angel Mode (epic transition effect)" << std::endl;
        std::cout << "\nCamera Controls:" << std::endl;
        std::cout << "WASD: Move camera" << std::endl;
        std::cout << "Q/E: Move camera up/down" << std::endl;
        std::cout << "Mouse Wheel: Zoom in/out" << std::endl;
        std::cout << "R/T: Rotate camera" << std::endl;
        std::cout << "Shift: Speed boost | Ctrl: Precision mode" << std::endl;
        std::cout << "Space: Reset camera to origin" << std::endl;
        std::cout << "C: Print camera info" << std::endl;
        std::cout << "\nGPU Compute Movement:" << std::endl;
        std::cout << "• All movement computed on GPU via compute shader" << std::endl;
        std::cout << "• Petal, orbit, and wave patterns supported" << std::endl;
        std::cout << "===============================================\n" << std::endl;
    }
    
    void processControls(World& world, bool& running, VulkanRenderer* renderer) {
        handleApplicationControls(world, running);
        handleEntityCreation(world, renderer);
        handlePerformanceControls(world, renderer);
        handleMovementTypeControls(world, renderer);
    }
    
    void handleApplicationControls(World& world, bool& running) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Handle quit conditions
        if (InputQuery::shouldQuit(flecsWorld) || 
            InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_ESCAPE)) {
            running = false;
        }
    }
    
    void handleEntityCreation(World& world, VulkanRenderer* renderer) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Add more GPU entities (stress test)
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_EQUALS) || 
            InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_KP_PLUS)) {
            std::cout << "Adding 1000 more GPU entities..." << std::endl;
            
            if (renderer && renderer->getGPUEntityManager()) {
                MovementType currentType = static_cast<MovementType>(g_currentMovementType);
                auto newEntities = world.getEntityFactory().createSwarmWithType(1000, glm::vec3(0.0f), 2.0f, currentType);
                renderer->getGPUEntityManager()->addEntitiesFromECS(newEntities);
                std::cout << "Total GPU entities now: " << renderer->getGPUEntityManager()->getEntityCount() << std::endl;
            } else {
                std::cerr << "Error: GPU entity manager not available!" << std::endl;
            }
        }
        
        // Create GPU entity at mouse position
        if (InputQuery::isMouseButtonPressed(flecsWorld, 0)) { // Left mouse button
            glm::vec2 mouseScreenPos = InputQuery::getMousePosition(flecsWorld);
            glm::vec2 mouseWorldPos = InputQuery::getMouseWorldPosition(flecsWorld);
            std::cout << "Mouse click - Screen: (" << mouseScreenPos.x << ", " << mouseScreenPos.y << ") -> World: (" << mouseWorldPos.x << ", " << mouseWorldPos.y << ")" << std::endl;
            
            if (renderer && renderer->getGPUEntityManager()) {
                // Create entity with current movement type
                MovementType currentType = static_cast<MovementType>(g_currentMovementType);
                auto mouseEntity = world.getEntityFactory().createMovingEntityWithType(
                    glm::vec3(mouseWorldPos.x, mouseWorldPos.y, 0.0f),
                    currentType
                );
                if (mouseEntity.is_valid()) {
                    std::vector<Entity> entityVec = {mouseEntity};
                    renderer->getGPUEntityManager()->addEntitiesFromECS(entityVec);
                    std::cout << "Created GPU entity with movement pattern" << std::endl;
                }
            } else {
                std::cerr << "Error: GPU entity manager not available!" << std::endl;
            }
        }
    }
    
    
    void handlePerformanceControls(World& world, VulkanRenderer* renderer) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Print performance report
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_P)) {
            Profiler::getInstance().printReport();
        }
        
        // Print current GPU stats
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_MINUS) || 
            InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_KP_MINUS)) {
            auto worldStats = world.getStats();
            float avgFrameTime = Profiler::getInstance().getFrameTime();
            float fps = avgFrameTime > 0.0f ? (1000.0f / avgFrameTime) : 0.0f;
            
            uint32_t gpuEntityCount = 0;
            if (renderer && renderer->getGPUEntityManager()) {
                gpuEntityCount = renderer->getGPUEntityManager()->getEntityCount();
            }
            
            std::cout << "Current Stats - CPU Entities: " << worldStats.memoryStats.activeEntities 
                      << ", GPU Entities: " << gpuEntityCount
                      << ", Frame Time: " << avgFrameTime << "ms"
                      << ", FPS: " << fps << std::endl;
        }
    }
    
    void handleMovementTypeControls(World& world, VulkanRenderer* renderer) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Handle Angel Mode toggle (CAPS LOCK)
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_CAPSLOCK)) {
            g_angelModeEnabled = !g_angelModeEnabled;
            std::cout << "Angel Mode " << (g_angelModeEnabled ? "ENABLED" : "DISABLED") 
                      << " - " << (g_angelModeEnabled ? "Biblical transitions via origin" : "Direct organic transitions") << std::endl;
        }
        
        // Handle movement type switching (0, 1, 2)
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_0)) {
            g_currentMovementType = 0;
            std::cout << "Movement type changed to: PETAL (0)" << std::endl;
            
            // Update all existing GPU entities
            if (renderer && renderer->getGPUEntityManager()) {
                renderer->getGPUEntityManager()->updateAllMovementTypes(0, g_angelModeEnabled);
            }
        }
        else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_1)) {
            g_currentMovementType = 1;
            std::cout << "Movement type changed to: ORBIT (1)" << std::endl;
            
            // Update all existing GPU entities
            if (renderer && renderer->getGPUEntityManager()) {
                renderer->getGPUEntityManager()->updateAllMovementTypes(1, g_angelModeEnabled);
            }
        }
        else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_2)) {
            g_currentMovementType = 2;
            std::cout << "Movement type changed to: WAVE (2)" << std::endl;
            
            // Update all existing GPU entities
            if (renderer && renderer->getGPUEntityManager()) {
                renderer->getGPUEntityManager()->updateAllMovementTypes(2, g_angelModeEnabled);
            }
        }
    }
    
}