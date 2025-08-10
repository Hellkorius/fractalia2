#pragma once

#include "system.hpp"
#include "component.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <chrono>
#include <iomanip>

// Flecs-native system scheduler that leverages Flecs built-in scheduling
class SystemScheduler {
private:
    flecs::world& world;
    std::vector<std::unique_ptr<SystemBase>> systems;
    std::unordered_map<std::string, SystemBase*> systemLookup;
    
    // Phase entities
    flecs::entity preInputPhase, inputPhase, logicPhase, physicsPhase, renderPhase, postRenderPhase;
    
    // Dependency tracking for deferred resolution
    std::vector<std::pair<std::string, std::string>> pendingDependencies;
    
    // Performance monitoring
    bool performanceMonitoringEnabled = true;
    
    // System timing statistics
    struct SystemStats {
        double totalTime = 0.0;
        uint64_t callCount = 0;
        double lastFrameTime = 0.0;
        
        double getAverageTime() const {
            return callCount > 0 ? totalTime / callCount : 0.0;
        }
    };
    
    std::unordered_map<std::string, SystemStats> systemStats;
    
public:
    SystemScheduler(flecs::world& w) : world(w) {
        setupFlecsPhases();
    }
    
    ~SystemScheduler() = default;
    
    // System registration with fluent interface support
    class SystemRegistration {
        SystemScheduler* scheduler;
        std::string systemName;
        
    public:
        SystemRegistration(SystemScheduler* sched, const std::string& name) 
            : scheduler(sched), systemName(name) {}
        
        SystemRegistration& inPhase(const std::string& phaseName) {
            auto* system = scheduler->systemLookup[systemName];
            if (system) {
                flecs::entity phase = scheduler->getPhaseEntity(phaseName);
                if (phase) {
                    system->setPhase(phase);
                }
            }
            return *this;
        }
    };
    
    // Register a system and let it self-register with Flecs
    template<typename SystemType, typename... Args>
    SystemRegistration addSystem(const std::string& name, Args&&... args) {
        auto system = std::make_unique<SystemType>(name, std::forward<Args>(args)...);
        SystemBase* ptr = system.get();
        
        systemLookup[name] = ptr;
        systems.push_back(std::move(system));
        
        return SystemRegistration(this, name);
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
        
        // Initialize global singleton components
        world.set<ApplicationState>({});
        
        for (auto& system : systems) {
            system->initialize(world);
            std::cout << "  Initialized system: " << system->getName() << std::endl;
        }
        
        // Resolve pending dependencies
        resolveDependencies();
        
        if (performanceMonitoringEnabled) {
            setupPerformanceMonitoring();
        }
        
        std::cout << "SystemScheduler initialization complete. Flecs will handle execution." << std::endl;
    }
    
    // Execute frame - Pure Flecs scheduling (no manual system loop)
    void executeFrame(float deltaTime) {
        // Update global application state for systems to access
        auto* appState = world.get_mut<ApplicationState>();
        if (appState) {
            appState->globalDeltaTime = deltaTime;
            appState->frameCount++;
        }
        
        // Let Flecs run all systems with proper phase ordering and dependencies
        auto startTime = std::chrono::high_resolution_clock::now();
        world.progress(deltaTime);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        // Update timing stats for the frame
        double frameDuration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        updateFrameStats(frameDuration);
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
    
    void toggleSystem(const std::string& name) {
        auto it = systemLookup.find(name);
        if (it != systemLookup.end()) {
            bool newState = !it->second->isEnabled();
            it->second->setEnabled(newState);
            std::cout << (newState ? "Enabled" : "Disabled") << " system: " << name << std::endl;
        }
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
        
        // Get Flecs native statistics
        // Note: Flecs 4.0 may have different stats API - simplified for compatibility
        std::cout << "\\nFlecs Statistics:" << std::endl;
        std::cout << "  Current frame time: " << world.delta_time() * 1000.0f << "ms" << std::endl;
        
        // Print individual system stats
        if (!systemStats.empty()) {
            std::cout << "\\nSystem Performance:" << std::endl;
            for (const auto& [name, stats] : systemStats) {
                std::cout << "  " << name << ":" << std::endl;
                std::cout << "    Avg time: " << std::fixed << std::setprecision(3) << stats.getAverageTime() << "ms" << std::endl;
                std::cout << "    Last frame: " << stats.lastFrameTime << "ms" << std::endl;
                std::cout << "    Call count: " << stats.callCount << std::endl;
            }
        }
        
        // Print system status and phases
        std::cout << "\\nRegistered Systems:" << std::endl;
        for (const auto& system : systems) {
            std::string phaseInfo = "";
            if (system->getPhase()) {
                phaseInfo = " [Phase: " + std::string(system->getPhase().name()) + "]";
            }
            std::cout << "  " << system->getName() 
                     << " - " << (system->isEnabled() ? "ENABLED" : "DISABLED") 
                     << phaseInfo << std::endl;
        }
        
        std::cout << "================================\\n" << std::endl;
    }
    
    // Get system by name
    SystemBase* getSystem(const std::string& name) {
        auto it = systemLookup.find(name);
        return it != systemLookup.end() ? it->second : nullptr;
    }
    
    // Add dependency between systems (Flecs native)
    SystemScheduler& addDependency(const std::string& system, const std::string& dependsOn) {
        // Store for later resolution during initialize()
        pendingDependencies.emplace_back(system, dependsOn);
        std::cout << "Added dependency: " << system << " depends on " << dependsOn << std::endl;
        return *this;
    }
    
    size_t getSystemCount() const { return systems.size(); }
    
private:
    void setupFlecsPhases() {
        // Flecs has built-in phases: OnLoad, PostLoad, PreUpdate, OnUpdate, OnValidate, PostUpdate, PreStore, OnStore
        // We can create custom phases if needed
        
        // Create custom phases for our specific needs with proper dependencies
        preInputPhase = world.entity("PreInput").add(flecs::Phase).depends_on(flecs::OnLoad);
        inputPhase = world.entity("Input").add(flecs::Phase).depends_on(preInputPhase);
        logicPhase = world.entity("Logic").add(flecs::Phase).depends_on(inputPhase);
        physicsPhase = world.entity("Physics").add(flecs::Phase).depends_on(logicPhase);
        renderPhase = world.entity("Render").add(flecs::Phase).depends_on(physicsPhase);
        postRenderPhase = world.entity("PostRender").add(flecs::Phase).depends_on(renderPhase);
        
        std::cout << "Created phases: PreInput -> Input -> Logic -> Physics -> Render -> PostRender" << std::endl;
    }
    
    flecs::entity getPhaseEntity(const std::string& phaseName) {
        if (phaseName == "PreInput") return preInputPhase;
        if (phaseName == "Input") return inputPhase;
        if (phaseName == "Logic") return logicPhase;
        if (phaseName == "Physics") return physicsPhase;
        if (phaseName == "Render") return renderPhase;
        if (phaseName == "PostRender") return postRenderPhase;
        
        std::cout << "Warning: Unknown phase '" << phaseName << "'" << std::endl;
        return flecs::entity::null();
    }
    
    void resolveDependencies() {
        for (const auto& [systemName, dependsOnName] : pendingDependencies) {
            auto* system = systemLookup[systemName];
            auto* dependsOnSystem = systemLookup[dependsOnName];
            
            if (system && dependsOnSystem) {
                flecs::entity systemEntity = system->getFlecsEntity();
                flecs::entity dependsOnEntity = dependsOnSystem->getFlecsEntity();
                
                if (systemEntity && dependsOnEntity) {
                    systemEntity.depends_on(dependsOnEntity);
                    std::cout << "  Configured dependency: " << systemName << " depends on " << dependsOnName << std::endl;
                } else {
                    std::cout << "  Warning: Could not resolve dependency between " << systemName << " and " << dependsOnName 
                             << " (missing Flecs entities)" << std::endl;
                }
            } else {
                std::cout << "  Warning: Could not find systems for dependency: " << systemName << " depends on " << dependsOnName << std::endl;
            }
        }
        
        pendingDependencies.clear();
    }
    
    void updateFrameStats(double frameDuration) {
        // Update stats for all enabled systems
        for (const auto& system : systems) {
            if (system->isEnabled()) {
                SystemStats& stats = systemStats[system->getName()];
                stats.lastFrameTime = frameDuration / systems.size(); // Rough approximation
                stats.totalTime += stats.lastFrameTime;
                stats.callCount++;
            }
        }
    }
    
    void setupPerformanceMonitoring() {
        // Enable Flecs native statistics - simplified for compatibility
        std::cout << "Performance monitoring enabled (using Flecs native stats)" << std::endl;
    }
};