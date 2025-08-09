#pragma once

#include "system.hpp"
#include "system_scheduler.hpp"
#include "systems/lifetime_system.hpp"
#include "systems/input_system.hpp" 
#include "systems/camera_system.hpp"
#include "systems/control_handler_system.hpp"
#include "../vulkan_renderer.h"
#include <memory>

// Central registry for all game systems using Flecs native scheduling
class SystemRegistry {
private:
    SystemScheduler scheduler;
    VulkanRenderer* renderer = nullptr;
    
public:
    SystemRegistry(flecs::world& world) : scheduler(world) {}
    
    // Register all game systems
    void registerAllSystems() {
        registerCoreECSSystems();
        registerInputSystems();
        registerCameraSystems();
        registerGameplaySystems();
    }
    
    void setRenderer(VulkanRenderer* r) {
        renderer = r;
    }
    
    void initialize() {
        scheduler.initialize();
    }
    
    void executeFrame(float deltaTime) {
        scheduler.executeFrame(deltaTime);
    }
    
    SystemScheduler& getScheduler() { return scheduler; }
    
private:
    void registerCoreECSSystems() {
        // Lifetime management system - Flecs handles this automatically
        scheduler.addSystem<FlecsSystem<Lifetime>>(
            "LifetimeSystem", 
            lifetime_system
        );
    }
    
    void registerInputSystems() {
        // Input processing system - runs on singleton input entity
        scheduler.addSystem<FlecsSystem<InputState, KeyboardInput, MouseInput, InputEvents>>(
            "InputSystem",
            input_processing_system
        );
        
        // Control handler - manual system for game controls
        scheduler.addSystem<ManualSystem>(
            "ControlHandler",
            [this](flecs::world& world, float deltaTime) {
                // Control handler functionality moved to main.cpp
                // This is a placeholder for future control system integration
                (void)world; (void)deltaTime;
                static int counter = 0;
                if (++counter % 300 == 0) { // Every 5 seconds at 60fps
                    // std::cout << "Control handler system running..." << std::endl;
                }
            }
        );
    }
    
    void registerCameraSystems() {
        // Camera control system - processes input and updates camera
        scheduler.addSystem<FlecsSystem<Camera>>(
            "CameraControlSystem",
            [](flecs::entity e, Camera& camera) {
                camera_control_system(e, camera, e.world().delta_time());
            }
        );
        
        // Camera matrix system - updates projection/view matrices
        scheduler.addSystem<FlecsSystem<Camera>>(
            "CameraMatrixSystem",
            camera_matrix_system
        );
    }
    
    void registerGameplaySystems() {
        // GPU entity upload system - manual system for uploading pending entities
        scheduler.addSystem<ManualSystem>(
            "GPUEntityUpload",
            [this](flecs::world& world, float deltaTime) {
                if (renderer && renderer->getGPUEntityManager()) {
                    renderer->uploadPendingGPUEntities();
                    renderer->setDeltaTime(deltaTime);
                }
            }
        );
        
        // Performance monitoring system
        scheduler.addSystem<ManualSystem>(
            "PerformanceMonitor",
            [this](flecs::world& world, float deltaTime) {
                static float accumTime = 0.0f;
                accumTime += deltaTime;
                
                // Print performance stats every 5 seconds
                if (accumTime >= 5.0f) {
                    scheduler.printPerformanceReport();
                    accumTime = 0.0f;
                }
            }
        );
    }
};