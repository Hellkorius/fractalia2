#pragma once

#include <flecs.h>
#include "entity.hpp"
#include "component.hpp"
#include <string>
#include <functional>

class SystemBase {
public:
    virtual ~SystemBase() = default;
    
    // Lifecycle methods
    virtual void initialize(flecs::world& world) = 0;
    virtual void update(flecs::world& world, float deltaTime) = 0;
    virtual void shutdown(flecs::world& world) {}
    
    // System metadata
    virtual std::string getName() const = 0;
    virtual bool isEnabled() const { return enabled; }
    virtual void setEnabled(bool enable) { enabled = enable; }
    
protected:
    bool enabled = true;
};

// Flecs system wrapper - automatically registers and manages Flecs systems
template<typename... Components>
class FlecsSystem : public SystemBase {
public:
    using SystemFunction = void(*)(flecs::entity, Components&...);
    using SystemLambda = std::function<void(flecs::entity, Components&...)>;
    
    // Constructor with function pointer
    FlecsSystem(const std::string& name, SystemFunction func) 
        : systemName(name), systemFunc(func), systemLambda(nullptr) {}
    
    // Constructor with lambda/std::function - template to avoid ambiguity
    template<typename Lambda>
    FlecsSystem(const std::string& name, Lambda&& lambda) 
        : systemName(name), systemFunc(nullptr), systemLambda(std::forward<Lambda>(lambda)) {}
    
    void initialize(flecs::world& world) override {
        if (systemFunc) {
            flecsSystem = world.system<Components...>(systemName.c_str()).each(systemFunc);
        } else if (systemLambda) {
            flecsSystem = world.system<Components...>(systemName.c_str()).each(systemLambda);
        }
    }
    
    void update(flecs::world& world, float deltaTime) override {
        // Flecs systems run automatically during world.progress()
        // This is called by scheduler for performance tracking
        (void)world; (void)deltaTime;
    }
    
    std::string getName() const override { return systemName; }
    
private:
    std::string systemName;
    SystemFunction systemFunc = nullptr;
    SystemLambda systemLambda;
    flecs::entity flecsSystem;
};

// Manual update system - for systems that need explicit control
class ManualSystem : public SystemBase {
public:
    using UpdateFunction = std::function<void(flecs::world&, float)>;
    
    ManualSystem(const std::string& name, UpdateFunction updateFunc)
        : systemName(name), updateFunction(updateFunc) {}
    
    void initialize(flecs::world& world) override {
        // Manual systems don't auto-register with Flecs
        (void)world;
    }
    
    void update(flecs::world& world, float deltaTime) override {
        if (enabled && updateFunction) {
            updateFunction(world, deltaTime);
        }
    }
    
    std::string getName() const override { return systemName; }
    
private:
    std::string systemName;
    UpdateFunction updateFunction;
};

// Utility system for one-time operations
class UtilitySystem : public SystemBase {
public:
    using UtilityFunction = std::function<void(flecs::world&)>;
    
    UtilitySystem(const std::string& name, UtilityFunction func)
        : systemName(name), utilityFunction(func) {}
    
    void initialize(flecs::world& world) override {
        if (utilityFunction) {
            utilityFunction(world);
        }
    }
    
    void update(flecs::world& world, float deltaTime) override {
        // Utility systems typically don't need updates
        (void)world; (void)deltaTime;
    }
    
    std::string getName() const override { return systemName; }
    
private:
    std::string systemName;
    UtilityFunction utilityFunction;
};

// Helper macros for creating systems
#define CREATE_FLECS_SYSTEM(name, function, ...) \
    std::make_unique<FlecsSystem<__VA_ARGS__>>(name, function)

#define CREATE_MANUAL_SYSTEM(name, function) \
    std::make_unique<ManualSystem>(name, function)

#define CREATE_UTILITY_SYSTEM(name, function) \
    std::make_unique<UtilitySystem>(name, function)
