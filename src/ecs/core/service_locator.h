#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>
#include <string>
#include <functional>
#include <mutex>

template<typename T>
concept Service = requires {
    typename T::ServiceTag;
};

class ServiceLocator {
public:
    static ServiceLocator& instance() {
        static ServiceLocator instance;
        return instance;
    }

    template<Service T>
    void registerService(std::shared_ptr<T> service) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_[std::type_index(typeid(T))] = service;
    }

    template<Service T, typename... Args>
    std::shared_ptr<T> createAndRegister(Args&&... args) {
        auto service = std::make_shared<T>(std::forward<Args>(args)...);
        registerService<T>(service);
        return service;
    }

    template<Service T>
    std::shared_ptr<T> getService() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(std::type_index(typeid(T)));
        if (it != services_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    template<Service T>
    T& requireService() {
        auto service = getService<T>();
        if (!service) {
            throw std::runtime_error("Required service not found: " + std::string(typeid(T).name()));
        }
        return *service;
    }

    template<Service T>
    bool hasService() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.find(std::type_index(typeid(T))) != services_.end();
    }

    void unregisterService(const std::type_index& type) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.erase(type);
    }

    template<Service T>
    void unregisterService() {
        unregisterService(std::type_index(typeid(T)));
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
    }

    void setDependencyInjector(std::function<void(const std::type_index&, std::shared_ptr<void>)> injector) {
        dependencyInjector_ = injector;
    }

private:
    ServiceLocator() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
    std::function<void(const std::type_index&, std::shared_ptr<void>)> dependencyInjector_;
};

#define DECLARE_SERVICE(ServiceClass) \
    struct ServiceTag {}; \
    static_assert(Service<ServiceClass>)

// Fix missing method - getService should be the correct name
#define IMPLEMENT_SERVICE(ServiceClass) \
    static_assert(Service<ServiceClass>)
