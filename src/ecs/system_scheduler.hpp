#pragma once

#include "system.hpp"
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <unordered_map>
#include <functional>

// System execution phases for dependency management
enum class SystemPhase {
    PreUpdate,      // Input handling, initialization
    EarlyUpdate,    // Entity lifecycle, spawning
    Update,         // Main game logic, physics
    LateUpdate,     // Animation, interpolation
    PreRender,      // Culling, batching
    Render,         // Actual rendering
    PostRender,     // UI, effects
    Cleanup         // Memory management, pooling
};

// System wrapper with metadata
struct SystemInfo {
    std::string name;
    SystemPhase phase;
    std::unique_ptr<SystemBase> system;
    std::vector<std::string> dependencies;
    std::vector<std::string> dependents;
    
    // Performance tracking
    std::chrono::high_resolution_clock::time_point lastExecutionTime;
    float averageExecutionTime{0.0f};
    size_t executionCount{0};
    bool enabled{true};
    
    SystemInfo(const std::string& n, SystemPhase p, std::unique_ptr<SystemBase> s)
        : name(n), phase(p), system(std::move(s)) {}
};

// High-performance system scheduler with dependency resolution
class SystemScheduler {
private:
    flecs::world& world;
    std::vector<std::unique_ptr<SystemInfo>> systems;
    std::unordered_map<std::string, SystemInfo*> systemLookup;
    std::vector<std::vector<SystemInfo*>> phaseExecutionOrder;
    bool needsReorder{false};
    
    // Global performance tracking
    std::chrono::high_resolution_clock::time_point frameStartTime;
    float totalFrameTime{0.0f};
    size_t frameCount{0};
    
public:
    SystemScheduler(flecs::world& w) : world(w) {
        phaseExecutionOrder.resize(static_cast<size_t>(SystemPhase::Cleanup) + 1);
    }
    
    // Register a system with dependencies
    template<typename SystemType, typename... Args>
    SystemScheduler& addSystem(const std::string& name, SystemPhase phase, Args&&... args) {
        auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
        auto systemInfo = std::make_unique<SystemInfo>(name, phase, std::move(system));
        
        SystemInfo* ptr = systemInfo.get();
        systemLookup[name] = ptr;
        systems.push_back(std::move(systemInfo));
        
        needsReorder = true;
        return *this;
    }
    
    // Add dependency relationship
    SystemScheduler& addDependency(const std::string& system, const std::string& dependsOn) {
        auto sysIt = systemLookup.find(system);
        auto depIt = systemLookup.find(dependsOn);
        
        if (sysIt != systemLookup.end() && depIt != systemLookup.end()) {
            sysIt->second->dependencies.push_back(dependsOn);
            depIt->second->dependents.push_back(system);
            needsReorder = true;
        }
        return *this;
    }
    
    // Initialize all systems
    void initialize() {
        for (auto& systemInfo : systems) {
            if (systemInfo->system) {
                systemInfo->system->initialize(world);
            }
        }
        reorderSystems();
    }
    
    // Execute all systems for current frame
    void executeFrame() {
        frameStartTime = std::chrono::high_resolution_clock::now();
        
        if (needsReorder) {
            reorderSystems();
        }
        
        // Execute systems in phase order
        for (const auto& phaseList : phaseExecutionOrder) {
            for (SystemInfo* systemInfo : phaseList) {
                if (systemInfo->enabled) {
                    executeSystem(*systemInfo);
                }
            }
        }
        
        // Update global frame statistics
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime).count();
        totalFrameTime = (totalFrameTime * frameCount + frameTime) / (frameCount + 1);
        frameCount++;
    }
    
    // Execute specific phase
    void executePhase(SystemPhase phase) {
        size_t phaseIndex = static_cast<size_t>(phase);
        if (phaseIndex < phaseExecutionOrder.size()) {
            for (SystemInfo* systemInfo : phaseExecutionOrder[phaseIndex]) {
                if (systemInfo->enabled) {
                    executeSystem(*systemInfo);
                }
            }
        }
    }
    
    // System control
    void enableSystem(const std::string& name, bool enabled = true) {
        auto it = systemLookup.find(name);
        if (it != systemLookup.end()) {
            it->second->enabled = enabled;
        }
    }
    
    void disableSystem(const std::string& name) {
        enableSystem(name, false);
    }
    
    // Performance monitoring
    struct PerformanceReport {
        std::string systemName;
        SystemPhase phase;
        float averageTime;
        size_t executionCount;
        bool enabled;
    };
    
    std::vector<PerformanceReport> getPerformanceReport() const {
        std::vector<PerformanceReport> report;
        report.reserve(systems.size());
        
        for (const auto& systemInfo : systems) {
            report.push_back({
                systemInfo->name,
                systemInfo->phase,
                systemInfo->averageExecutionTime,
                systemInfo->executionCount,
                systemInfo->enabled
            });
        }
        
        return report;
    }
    
    float getAverageFrameTime() const { return totalFrameTime; }
    size_t getFrameCount() const { return frameCount; }
    
    // Get system by name for direct access
    SystemInfo* getSystem(const std::string& name) {
        auto it = systemLookup.find(name);
        return it != systemLookup.end() ? it->second : nullptr;
    }
    
private:
    void executeSystem(SystemInfo& systemInfo) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Here we'd call the system's update method
        // Since SystemBase doesn't have an update method, we'd need to extend it
        // For now, this is a placeholder for the execution logic
        
        auto endTime = std::chrono::high_resolution_clock::now();
        float executionTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        
        // Update performance statistics
        systemInfo.averageExecutionTime = 
            (systemInfo.averageExecutionTime * systemInfo.executionCount + executionTime) / 
            (systemInfo.executionCount + 1);
        systemInfo.executionCount++;
        systemInfo.lastExecutionTime = endTime;
    }
    
    void reorderSystems() {
        // Clear existing order
        for (auto& phaseList : phaseExecutionOrder) {
            phaseList.clear();
        }
        
        // Group systems by phase and resolve dependencies within each phase
        for (auto& systemInfo : systems) {
            size_t phaseIndex = static_cast<size_t>(systemInfo->phase);
            if (phaseIndex < phaseExecutionOrder.size()) {
                phaseExecutionOrder[phaseIndex].push_back(systemInfo.get());
            }
        }
        
        // Sort each phase by dependencies (topological sort)
        for (auto& phaseList : phaseExecutionOrder) {
            topologicalSort(phaseList);
        }
        
        needsReorder = false;
    }
    
    void topologicalSort(std::vector<SystemInfo*>& systems) {
        // Simple dependency-based ordering
        // For a more robust implementation, use proper topological sort algorithm
        std::sort(systems.begin(), systems.end(), 
                 [](const SystemInfo* a, const SystemInfo* b) {
                     return a->dependencies.size() < b->dependencies.size();
                 });
    }
};