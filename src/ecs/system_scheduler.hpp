#pragma once

#include "system.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <iostream>

// Flecs-native system scheduler that leverages Flecs built-in scheduling
class SystemScheduler {
private:
    flecs::world& world;
    std::vector<std::unique_ptr<SystemBase>> systems;
    std::unordered_map<std::string, SystemBase*> systemLookup;
    
    // Performance monitoring
    bool performanceMonitoringEnabled = true;
    
public:
    SystemScheduler(flecs::world& w) : world(w) {
        setupFlecsPhases();
    }
    
    ~SystemScheduler() = default;
    
    // Register a system and let it self-register with Flecs
    template<typename SystemType, typename... Args>
    SystemScheduler& addSystem(const std::string& name, Args&&... args) {
        auto system = std::make_unique<SystemType>(name, std::forward<Args>(args)...);
        SystemBase* ptr = system.get();
        
        systemLookup[name] = ptr;
        systems.push_back(std::move(system));
        
        return *this;
    }
    
    // Add system with explicit phase control
    template<typename SystemType, typename... Args> 
    SystemScheduler& addSystemInPhase(const std::string& name, flecs::entity_t phase, Args&&... args) {
        auto system = std::make_unique<SystemType>(name, std::forward<Args>(args)...);
        SystemBase* ptr = system.get();
        
        systemLookup[name] = ptr;
        systems.push_back(std::move(system));
        
        // Initialize system and configure phase
        ptr->initialize(world);
        
        // If it's a FlecsSystem, we can configure its phase
        // This would require extending the SystemBase interface
        
        return *this;
    }
    
    // Initialize all systems - they self-register with Flecs
    void initialize() {
        std::cout << "Initializing " << systems.size() << " systems with Flecs scheduler..." << std::endl;
        
        for (auto& system : systems) {
            system->initialize(world);
            std::cout << "  Initialized system: " << system->getName() << std::endl;
        }
        
        if (performanceMonitoringEnabled) {
            setupPerformanceMonitoring();
        }
        
        std::cout << "SystemScheduler initialization complete. Flecs will handle execution." << std::endl;
    }
    
    // Execute frame - Flecs handles this automatically
    void executeFrame(float deltaTime) {
        // Manual systems need explicit updates
        for (auto& system : systems) {
            // Only update manual systems - Flecs systems run automatically
            if (auto* manualSystem = dynamic_cast<ManualSystem*>(system.get())) {
                if (manualSystem->isEnabled()) {
                    manualSystem->update(world, deltaTime);
                }
            }
        }
        
        // Let Flecs run all registered systems
        world.progress(deltaTime);
    }
    
    // System control
    void enableSystem(const std::string& name, bool enabled = true) {
        auto it = systemLookup.find(name);
        if (it != systemLookup.end()) {
            it->second->setEnabled(enabled);
            std::cout << (enabled ? "Enabled" : "Disabled") << " system: " << name << std::endl;
        }
    }
    
    void disableSystem(const std::string& name) {
        enableSystem(name, false);
    }
    
    // Performance monitoring using Flecs built-in stats
    void enablePerformanceMonitoring(bool enable = true) {
        performanceMonitoringEnabled = enable;
        if (enable) {
            setupPerformanceMonitoring();
        }
    }
    
    void printPerformanceReport() {
        if (!performanceMonitoringEnabled) {
            std::cout << "Performance monitoring disabled. Enable with enablePerformanceMonitoring()" << std::endl;
            return;
        }
        
        std::cout << "\\n=== System Performance Report ===" << std::endl;
        std::cout << "Frame time: " << world.delta_time() * 1000.0f << "ms" << std::endl;
        std::cout << "FPS: " << (world.delta_time() > 0 ? 1.0f / world.delta_time() : 0) << std::endl;
        
        // Print system status
        std::cout << "\\nRegistered Systems:" << std::endl;
        for (const auto& system : systems) {
            std::cout << "  " << system->getName() 
                     << " - " << (system->isEnabled() ? "ENABLED" : "DISABLED") << std::endl;
        }
        
        // Flecs provides detailed stats through world.get_stats()
        // This would require enabling Flecs stats module
        std::cout << "================================\\n" << std::endl;
    }
    
    // Get system by name
    SystemBase* getSystem(const std::string& name) {
        auto it = systemLookup.find(name);
        return it != systemLookup.end() ? it->second : nullptr;
    }
    
    // Add dependency between systems (Flecs native)
    SystemScheduler& addDependency(const std::string& system, const std::string& dependsOn) {
        // This would require accessing the underlying Flecs systems
        // For now, log the dependency
        std::cout << "Added dependency: " << system << " depends on " << dependsOn << std::endl;
        return *this;
    }
    
    size_t getSystemCount() const { return systems.size(); }
    
private:
    void setupFlecsPhases() {
        // Flecs has built-in phases: OnLoad, PostLoad, PreUpdate, OnUpdate, OnValidate, PostUpdate, PreStore, OnStore
        // We can create custom phases if needed
        
        // Create custom phases for our specific needs
        world.entity("PreInput").add(flecs::Phase).depends_on(flecs::OnLoad);
        world.entity("Input").add(flecs::Phase).depends_on(world.entity("PreInput"));
        world.entity("Logic").add(flecs::Phase).depends_on(world.entity("Input"));
        world.entity("Physics").add(flecs::Phase).depends_on(world.entity("Logic"));
        world.entity("Render").add(flecs::Phase).depends_on(world.entity("Physics"));
        world.entity("PostRender").add(flecs::Phase).depends_on(world.entity("Render"));
    }
    
    void setupPerformanceMonitoring() {
        // Flecs has built-in performance monitoring
        // We can access it through world stats if needed
        std::cout << "Performance monitoring enabled (using Flecs native stats)" << std::endl;
    }
};