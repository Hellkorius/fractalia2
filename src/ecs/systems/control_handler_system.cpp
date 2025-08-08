#include "control_handler_system.hpp"
#include "input_system.hpp"
#include "../../vulkan_renderer.h"
#include "../gpu_entity_manager.h"
#include <iostream>

namespace ControlHandler {
    
    void initialize(World& world) {
        std::cout << "\n=== GPU Compute Movement Demo Controls ===" << std::endl;
        std::cout << "ESC: Exit" << std::endl;
        std::cout << "P: Print detailed performance report" << std::endl;
        std::cout << "+/=: Add 1000 more GPU entities" << std::endl;
        std::cout << "-: Show current GPU performance stats" << std::endl;
        std::cout << "Left Click: Create GPU entity with movement at mouse position" << std::endl;
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
                auto newEntities = world.getEntityFactory().createSwarm(1000, glm::vec3(0.0f), 2.0f);
                renderer->getGPUEntityManager()->addEntitiesFromECS(newEntities);
                std::cout << "Total GPU entities now: " << renderer->getGPUEntityManager()->getEntityCount() << std::endl;
            } else {
                std::cerr << "Error: GPU entity manager not available!" << std::endl;
            }
        }
        
        // Create GPU entity at mouse position
        if (InputQuery::isMouseButtonPressed(flecsWorld, 0)) { // Left mouse button
            glm::vec2 mouseWorldPos = InputQuery::getMouseWorldPosition(flecsWorld);
            std::cout << "Creating GPU entity at mouse position: " << mouseWorldPos.x << ", " << mouseWorldPos.y << std::endl;
            
            if (renderer && renderer->getGPUEntityManager()) {
                auto mouseEntity = world.getEntityFactory().createMovingEntity(
                    glm::vec3(mouseWorldPos.x, mouseWorldPos.y, 0.0f)
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
    
}