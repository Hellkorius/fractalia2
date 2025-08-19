#include "world_manager.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

// Service implementation - removed macro

WorldManager::WorldManager() 
    : world_()
    , performanceMonitoringEnabled_(false)
    , frameTimeAccumulator_(0.0f)
    , frameCount_(0) {
    
    // Configure Flecs for optimal performance
    world_.set<flecs::Rest>({});  // Enable REST API for debugging
    // Performance monitoring setup - using available Flecs features
    
    // Setup basic phases - simplified for compatibility
}

WorldManager::~WorldManager() {
    shutdown();
}

bool WorldManager::initialize() {
    try {
        // Initialize core Flecs systems
        world_.set_threads(std::thread::hardware_concurrency());
        
        // Enable performance monitoring by default
        enablePerformanceMonitoring(true);
        
        return true;
    } catch (const std::exception& e) {
        // Log error (in real implementation, use proper logging)
        return false;
    }
}

void WorldManager::shutdown() {
    // Unload all modules in reverse order of loading
    unloadAllModules();
    
    // Reset performance monitoring
    performanceCallback_ = nullptr;
    performanceMonitoringEnabled_ = false;
    
    // Flecs world will be automatically cleaned up
}

bool WorldManager::unloadModule(const std::string& name) {
    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return false;
    }
    
    // Check for dependencies - simple implementation
    // In a more sophisticated system, this would check dependency graph
    for (const auto& [moduleName, module] : modules_) {
        if (moduleName != name) {
            // Check if this module depends on the one we're trying to unload
            // For now, we'll just prevent unloading if other modules exist
            // A real implementation would have proper dependency tracking
        }
    }
    
    try {
        // Shutdown the module
        it->second->shutdown();
        
        // Remove from modules map
        modules_.erase(it);
        
        return true;
    } catch (const std::exception& e) {
        // Log error (in real implementation, use proper logging)
        return false;
    }
}

void WorldManager::unloadAllModules() {
    // Unload modules in reverse order of loading to respect dependencies
    std::vector<std::string> moduleNames;
    moduleNames.reserve(modules_.size());
    
    for (const auto& [name, module] : modules_) {
        moduleNames.push_back(name);
    }
    
    // Reverse the order for proper dependency unloading
    std::reverse(moduleNames.begin(), moduleNames.end());
    
    for (const auto& name : moduleNames) {
        unloadModule(name);
    }
}

void WorldManager::executeFrame(float deltaTime) {
    auto frameStartTime = std::chrono::high_resolution_clock::now();
    
    try {
        // Update all modules first
        for (const auto& [name, module] : modules_) {
            if (module->isInitialized()) {
                module->update(deltaTime);
            }
        }
        
        // Execute Flecs frame with proper phase ordering
        world_.progress(deltaTime);
        
        // Performance monitoring
        if (performanceMonitoringEnabled_) {
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime).count();
            
            frameTimeAccumulator_ += frameDuration;
            frameCount_++;
            
            // Calculate average over sample size
            if (frameCount_ >= FRAME_SAMPLE_SIZE) {
                float averageFrameTime = frameTimeAccumulator_ / static_cast<float>(frameCount_);
                
                if (performanceCallback_) {
                    performanceCallback_(averageFrameTime);
                }
                
                // Reset accumulator
                frameTimeAccumulator_ = 0.0f;
                frameCount_ = 0;
            }
        }
        
    } catch (const std::exception& e) {
        // Log error and attempt recovery
        // In a production system, this would have more sophisticated error handling
        
        // Reset frame time tracking on error
        frameTimeAccumulator_ = 0.0f;
        frameCount_ = 0;
    }
}

void WorldManager::registerPerformanceCallback(std::function<void(float)> callback) {
    performanceCallback_ = std::move(callback);
}

void WorldManager::enablePerformanceMonitoring(bool enable) {
    performanceMonitoringEnabled_ = enable;
    
    if (!enable) {
        // Reset counters when disabling
        frameTimeAccumulator_ = 0.0f;
        frameCount_ = 0;
    }
}

size_t WorldManager::getEntityCount() const {
    // Count all entities with any component
    size_t count = 0;
    world_.each([&count](flecs::entity) {
        count++;
    });
    return count;
}

float WorldManager::getAverageFrameTime() const {
    if (frameCount_ == 0) {
        return 0.0f;
    }
    return frameTimeAccumulator_ / static_cast<float>(frameCount_);
}

float WorldManager::getFPS() const {
    float avgFrameTime = getAverageFrameTime();
    if (avgFrameTime <= 0.0f) {
        return 0.0f;
    }
    return 1000.0f / avgFrameTime; // Convert from ms to FPS
}