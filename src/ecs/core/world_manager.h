#pragma once

#include <flecs.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include "service_locator.h"

class ECSModule;

class WorldManager {
public:
    DECLARE_SERVICE(WorldManager);

    explicit WorldManager();
    ~WorldManager();

    bool initialize();
    void shutdown();

    flecs::world& getWorld() { return world_; }
    const flecs::world& getWorld() const { return world_; }

    template<typename ModuleType, typename... Args>
    std::shared_ptr<ModuleType> loadModule(const std::string& name, Args&&... args) {
        static_assert(std::is_base_of_v<ECSModule, ModuleType>, "Module must inherit from ECSModule");
        
        if (modules_.find(name) != modules_.end()) {
            return std::static_pointer_cast<ModuleType>(modules_[name]);
        }

        auto module = std::make_shared<ModuleType>(std::forward<Args>(args)...);
        if (module->initialize(world_)) {
            modules_[name] = module;
            return module;
        }
        
        return nullptr;
    }

    template<typename ModuleType>
    std::shared_ptr<ModuleType> getModule(const std::string& name) {
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            return std::static_pointer_cast<ModuleType>(it->second);
        }
        return nullptr;
    }

    bool unloadModule(const std::string& name);
    void unloadAllModules();

    void executeFrame(float deltaTime);
    
    void registerSystems();

    void registerPerformanceCallback(std::function<void(float)> callback);
    void enablePerformanceMonitoring(bool enable);

    size_t getEntityCount() const;
    float getAverageFrameTime() const;
    float getFPS() const;

private:
    flecs::world world_;
    std::unordered_map<std::string, std::shared_ptr<ECSModule>> modules_;
    
    bool performanceMonitoringEnabled_ = false;
    std::function<void(float)> performanceCallback_;
    
    float frameTimeAccumulator_ = 0.0f;
    size_t frameCount_ = 0;
    static constexpr size_t FRAME_SAMPLE_SIZE = 60;
};

class ECSModule {
public:
    virtual ~ECSModule() = default;
    
    virtual bool initialize(flecs::world& world) = 0;
    virtual void shutdown() = 0;
    virtual void update(float deltaTime) {}
    
    virtual const std::string& getName() const = 0;
    virtual bool isInitialized() const { return initialized_; }

protected:
    bool initialized_ = false;
};