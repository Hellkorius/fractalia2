#pragma once

#include <flecs.h>
#include "component.h"
#include <iostream>
#include <string>
#include <chrono>

// Simplified phase manager for direct Flecs system registration
// Usage: world.system<Transform>().each(func).child_of(scheduler.getInputPhase())
class SystemScheduler {
private:
    flecs::world& world;
    
    // Phase entities for system ordering
    flecs::entity preInputPhase, inputPhase, logicPhase, physicsPhase, renderPhase, postRenderPhase;
    
    // Performance monitoring
    bool performanceMonitoringEnabled = true;
    std::chrono::steady_clock::time_point lastReportTime;
    
public:
    SystemScheduler(flecs::world& w) : world(w) {
        setupFlecsPhases();
        lastReportTime = std::chrono::steady_clock::now();
    }
    
    ~SystemScheduler() = default;
    
    // Phase access for direct system registration
    flecs::entity getPreInputPhase() const { return preInputPhase; }
    flecs::entity getInputPhase() const { return inputPhase; }
    flecs::entity getLogicPhase() const { return logicPhase; }
    flecs::entity getPhysicsPhase() const { return physicsPhase; }
    flecs::entity getRenderPhase() const { return renderPhase; }
    flecs::entity getPostRenderPhase() const { return postRenderPhase; }
    
    // Convenience method to get phase by name
    flecs::entity getPhase(const std::string& phaseName) {
        if (phaseName == "PreInput") return preInputPhase;
        if (phaseName == "Input") return inputPhase;
        if (phaseName == "Logic") return logicPhase;
        if (phaseName == "Physics") return physicsPhase;
        if (phaseName == "Render") return renderPhase;
        if (phaseName == "PostRender") return postRenderPhase;
        
        std::cerr << "Warning: Unknown phase '" << phaseName << "'" << std::endl;
        return flecs::entity::null();
    }
    
    // Initialize global singleton components
    void initialize() {
        std::cout << "SystemScheduler: Phases created, ready for direct system registration" << std::endl;
        
        // Initialize global singleton components only if they don't exist
        if (!world.has<ApplicationState>()) {
            world.set<ApplicationState>({});
        }
        
        if (performanceMonitoringEnabled) {
            std::cout << "SystemScheduler: Performance monitoring enabled" << std::endl;
        }
    }
    
    // Execute frame - Pure Flecs scheduling
    void executeFrame(float deltaTime) {
        // Update global application state
        auto* appState = world.get_mut<ApplicationState>();
        if (appState) {
            appState->globalDeltaTime = deltaTime;
            appState->frameCount++;
        }
        
        // Let Flecs run all systems with proper phase ordering
        world.progress(deltaTime);
    }
    
    // Performance monitoring
    void enablePerformanceMonitoring(bool enable = true) {
        performanceMonitoringEnabled = enable;
    }
    
    void printPerformanceReport() {
        if (!performanceMonitoringEnabled) {
            std::cout << "Performance monitoring disabled" << std::endl;
            return;
        }
        
        std::cout << "\n=== System Performance Report (Simplified) ===" << std::endl;
        std::cout << "Frame time: " << world.delta_time() * 1000.0f << "ms" << std::endl;
        std::cout << "FPS: " << (world.delta_time() > 0 ? 1.0f / world.delta_time() : 0) << std::endl;
        
        std::cout << "\nPhase Structure:" << std::endl;
        std::cout << "  PreInput -> Input -> Logic -> Physics -> Render -> PostRender" << std::endl;
        
        std::cout << "\nNote: Systems are registered directly with Flecs phases" << std::endl;
        std::cout << "Use world.system<Components>().each(func).child_of(phase)" << std::endl;
        std::cout << "===============================================\n" << std::endl;
    }
    
private:
    void setupFlecsPhases() {
        // Create custom phases with proper dependencies
        preInputPhase = world.entity("PreInput").add(flecs::Phase).depends_on(flecs::OnLoad);
        inputPhase = world.entity("Input").add(flecs::Phase).depends_on(preInputPhase);
        logicPhase = world.entity("Logic").add(flecs::Phase).depends_on(inputPhase);
        physicsPhase = world.entity("Physics").add(flecs::Phase).depends_on(logicPhase);
        renderPhase = world.entity("Render").add(flecs::Phase).depends_on(physicsPhase);
        postRenderPhase = world.entity("PostRender").add(flecs::Phase).depends_on(renderPhase);
        
        std::cout << "SystemScheduler: Created phases PreInput -> Input -> Logic -> Physics -> Render -> PostRender" << std::endl;
    }
};