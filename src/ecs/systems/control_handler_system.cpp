#include "control_handler_system.hpp"
#include "input_system.hpp"
#include <iostream>

namespace ControlHandler {
    
    void initialize(World& world) {
        std::cout << "\n=== Fractal ECS Demo Controls ===" << std::endl;
        std::cout << "ESC: Exit" << std::endl;
        std::cout << "P: Print detailed performance report" << std::endl;
        std::cout << "+/=: Add 100 more fractal entities" << std::endl;
        std::cout << "-: Show current performance stats" << std::endl;
        std::cout << "Left Click: Create entity with fractal movement at mouse position" << std::endl;
        std::cout << "\nCamera Controls:" << std::endl;
        std::cout << "WASD: Move camera" << std::endl;
        std::cout << "Q/E: Move camera up/down" << std::endl;
        std::cout << "Mouse Wheel: Zoom in/out" << std::endl;
        std::cout << "R/T: Rotate camera" << std::endl;
        std::cout << "Shift: Speed boost | Ctrl: Precision mode" << std::endl;
        std::cout << "Space: Reset camera to origin" << std::endl;
        std::cout << "C: Print camera info" << std::endl;
        std::cout << "\nFractal Movement Patterns Active:" << std::endl;
        std::cout << "• Linear, Orbital, Spiral, Lissajous" << std::endl;
        std::cout << "• Brownian, Fractal, Wave, Petal, Butterfly" << std::endl;
        std::cout << "=====================================\n" << std::endl;
    }
    
    void processControls(World& world, bool& running) {
        handleApplicationControls(world, running);
        handleEntityCreation(world);
        handlePatternSwitching(world);
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
            std::cout << "Adding 100 more fractal entities..." << std::endl;
            auto newEntities = world.getEntityFactory().createSwarm(100, glm::vec3(0.0f), 1.5f);
            auto worldStats = world.getStats();
            std::cout << "Total entities now: " << worldStats.memoryStats.activeEntities << std::endl;
        }
        
        // Create entity at mouse position
        if (InputQuery::isMouseButtonPressed(flecsWorld, 0)) { // Left mouse button
            glm::vec2 mouseWorldPos = InputQuery::getMouseWorldPosition(flecsWorld);
            std::cout << "Creating fractal entity at mouse position: " << mouseWorldPos.x << ", " << mouseWorldPos.y << std::endl;
            
            auto mouseEntity = world.getEntityFactory().createFractalEntity(
                glm::vec3(mouseWorldPos.x, mouseWorldPos.y, 0.0f)
            );
            if (mouseEntity.is_valid()) {
                std::cout << "Created entity with fractal movement pattern" << std::endl;
            }
        }
    }
    
    void handlePatternSwitching(World& world) {
        flecs::world& flecsWorld = world.getFlecsWorld();
        
        // Sacred Geometry pattern switching with number keys (1-0)
        if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_1)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::FlowerOfLife, "Flower of Life");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_2)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::SeedOfLife, "Seed of Life");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_3)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::VesicaPiscis, "Vesica Piscis");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_4)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::SriYantra, "Sri Yantra");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_5)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::PlatonicSolids, "Platonic Solids");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_6)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::FibonacciSpiral, "Fibonacci Spiral");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_7)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::GoldenRatio, "Golden Ratio");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_8)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::Metatron, "Metatron's Cube");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_9)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::TreeOfLife, "Tree of Life");
        } else if (InputQuery::isKeyPressed(flecsWorld, SDL_SCANCODE_0)) {
            switchAllEntitiesToPattern(flecsWorld, MovementType::TetraktysFlow, "Tetraktys Flow");
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
    
    void switchAllEntitiesToPattern(flecs::world& world, MovementType pattern, const std::string& patternName) {
        std::cout << "Switching all entities to " << patternName << " pattern around origin..." << std::endl;
        world.each([pattern](flecs::entity e, MovementPattern& movementPattern) {
            movementPattern.type = pattern;
            movementPattern.center = glm::vec3(0.0f, 0.0f, 0.0f); // Set center to origin
            movementPattern.initialized = false; // Reset pattern initialization
        });
    }
}