#pragma once

#include <flecs.h>
#include <functional>
#include <string>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <iomanip>
#include "../core/service_locator.h"

namespace SystemHelpers {

template<typename... Components>
class SystemBuilder {
private:
    flecs::world& world_;
    std::string name_;
    flecs::entity phase_;
    bool enabled_ = true;
    
public:
    SystemBuilder(flecs::world& world, const std::string& name) 
        : world_(world), name_(name) {}
    
    SystemBuilder& inPhase(flecs::entity phase) {
        phase_ = phase;
        return *this;
    }
    
    SystemBuilder& enabled(bool enabled) {
        enabled_ = enabled;
        return *this;
    }
    
    template<typename Func>
    flecs::system buildEach(Func&& func) {
        auto system = world_.system<Components...>(name_.c_str())
            .each(std::forward<Func>(func));
        
        if (phase_.is_valid()) {
            system.child_of(phase_);
        }
        
        if (!enabled_) {
            system.disable();
        }
        
        return system;
    }
    
    template<typename Func>
    flecs::system buildIter(Func&& func) {
        auto system = world_.system<Components...>(name_.c_str())
            .iter(std::forward<Func>(func));
        
        if (phase_.is_valid()) {
            system.child_of(phase_);
        }
        
        if (!enabled_) {
            system.disable();
        }
        
        return system;
    }
};

template<typename... Components>
SystemBuilder<Components...> createSystem(flecs::world& world, const std::string& name) {
    return SystemBuilder<Components...>(world, name);
}

class SystemManager {
private:
    flecs::world& world_;
    std::unordered_map<std::string, flecs::system> systems_;
    
public:
    explicit SystemManager(flecs::world& world) : world_(world) {}
    
    template<typename... Components, typename Func>
    void registerSystem(const std::string& name, flecs::entity phase, Func&& func, bool enabled = true) {
        auto system = createSystem<Components...>(world_, name)
            .inPhase(phase)
            .enabled(enabled)
            .buildEach(std::forward<Func>(func));
        
        systems_[name] = system;
    }
    
    void enableSystem(const std::string& name, bool enabled = true) {
        auto it = systems_.find(name);
        if (it != systems_.end()) {
            if (enabled) {
                it->second.enable();
            } else {
                it->second.disable();
            }
        }
    }
    
    void removeSystem(const std::string& name) {
        auto it = systems_.find(name);
        if (it != systems_.end()) {
            it->second.destruct();
            systems_.erase(it);
        }
    }
    
    bool hasSystem(const std::string& name) const {
        return systems_.find(name) != systems_.end();
    }
    
    flecs::system getSystem(const std::string& name) const {
        auto it = systems_.find(name);
        return it != systems_.end() ? it->second : flecs::system{};
    }
    
    void clear() {
        for (auto& [name, system] : systems_) {
            system.destruct();
        }
        systems_.clear();
    }
    
    size_t getSystemCount() const {
        return systems_.size();
    }
    
    std::vector<std::string> getSystemNames() const {
        std::vector<std::string> names;
        names.reserve(systems_.size());
        for (const auto& [name, system] : systems_) {
            names.push_back(name);
        }
        return names;
    }
};

class PerformanceProfiler {
private:
    struct SystemProfile {
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::duration<float, std::milli> total_time{0};
        size_t call_count = 0;
        float average_time = 0.0f;
    };
    
    std::unordered_map<std::string, SystemProfile> profiles_;
    bool enabled_ = false;
    
public:
    void enable(bool enabled = true) {
        enabled_ = enabled;
    }
    
    void startSystem(const std::string& name) {
        if (!enabled_) return;
        
        auto& profile = profiles_[name];
        profile.name = name;
        profile.start_time = std::chrono::high_resolution_clock::now();
    }
    
    void endSystem(const std::string& name) {
        if (!enabled_) return;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto& profile = profiles_[name];
        
        auto duration = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
            end_time - profile.start_time);
        
        profile.total_time += duration;
        profile.call_count++;
        profile.average_time = profile.total_time.count() / profile.call_count;
    }
    
    void reset() {
        profiles_.clear();
    }
    
    void printReport() const {
        if (!enabled_ || profiles_.empty()) {
            std::cout << "Performance profiling disabled or no data available" << std::endl;
            return;
        }
        
        std::cout << "\n=== System Performance Report ===" << std::endl;
        std::cout << "System Name                   | Avg Time (ms) | Calls | Total (ms)" << std::endl;
        std::cout << "-----------------------------------------------------------" << std::endl;
        
        for (const auto& [name, profile] : profiles_) {
            std::cout << std::left << std::setw(30) << name 
                      << "| " << std::setw(12) << std::fixed << std::setprecision(3) << profile.average_time
                      << "| " << std::setw(6) << profile.call_count
                      << "| " << std::setw(10) << profile.total_time.count() << std::endl;
        }
        std::cout << "=========================================" << std::endl;
    }
    
    float getSystemAverageTime(const std::string& name) const {
        auto it = profiles_.find(name);
        return it != profiles_.end() ? it->second.average_time : 0.0f;
    }
    
    size_t getSystemCallCount(const std::string& name) const {
        auto it = profiles_.find(name);
        return it != profiles_.end() ? it->second.call_count : 0;
    }
};

class ScopedSystemProfiler {
private:
    PerformanceProfiler& profiler_;
    std::string system_name_;
    
public:
    ScopedSystemProfiler(PerformanceProfiler& profiler, const std::string& name)
        : profiler_(profiler), system_name_(name) {
        profiler_.startSystem(system_name_);
    }
    
    ~ScopedSystemProfiler() {
        profiler_.endSystem(system_name_);
    }
};

#define PROFILE_SYSTEM(profiler, name) \
    ScopedSystemProfiler _prof(profiler, name)

template<typename ServiceType>
class ServiceSystemBuilder {
private:
    flecs::world& world_;
    std::string name_;
    flecs::entity phase_;
    
public:
    ServiceSystemBuilder(flecs::world& world, const std::string& name)
        : world_(world), name_(name) {}
    
    ServiceSystemBuilder& inPhase(flecs::entity phase) {
        phase_ = phase;
        return *this;
    }
    
    template<typename... Components, typename Func>
    flecs::system build(Func&& func) {
        auto& service = ServiceLocator::instance().requireService<ServiceType>();
        
        auto system = world_.system<Components...>(name_.c_str())
            .each([&service, func = std::forward<Func>(func)](flecs::entity e, Components&... components) {
                func(e, service, components...);
            });
        
        if (phase_.is_valid()) {
            system.child_of(phase_);
        }
        
        return system;
    }
};

template<typename ServiceType>
ServiceSystemBuilder<ServiceType> createServiceSystem(flecs::world& world, const std::string& name) {
    return ServiceSystemBuilder<ServiceType>(world, name);
}

namespace SystemRegistry {
    inline void registerCommonSystems(flecs::world& world, SystemManager& manager) {
        auto preInputPhase = world.entity("PreInput");
        auto inputPhase = world.entity("Input");
        auto logicPhase = world.entity("Logic");
        auto physicsPhase = world.entity("Physics");
        auto renderPhase = world.entity("Render");
        
        manager.registerSystem<Transform>("transform_update", logicPhase,
            [](flecs::entity e, Transform& transform) {
                if (transform.dirty) {
                    transform.getMatrix();
                }
            });
        
        manager.registerSystem<Lifetime>("lifetime_update", logicPhase,
            [](flecs::entity e, Lifetime& lifetime) {
                if (lifetime.maxAge > 0) {
                    lifetime.currentAge += e.world().delta_time();
                    if (lifetime.autoDestroy && lifetime.currentAge >= lifetime.maxAge) {
                        e.destruct();
                    }
                }
            });
        
        manager.registerSystem<Transform, Velocity>("velocity_update", physicsPhase,
            [](flecs::entity e, Transform& transform, Velocity& velocity) {
                float dt = e.world().delta_time();
                transform.setPosition(transform.position + velocity.linear * dt);
                transform.setRotation(transform.rotation + velocity.angular * dt);
            });
    }
}

} // namespace SystemHelpers