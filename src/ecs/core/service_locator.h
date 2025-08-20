#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <stdexcept>
#include <iostream>

template<typename T>
concept Service = std::is_class_v<T>;

// Service lifecycle stages
enum class ServiceLifecycle {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    SHUTTING_DOWN,
    SHUTDOWN
};

// Service metadata for dependency management
struct ServiceMetadata {
    std::type_index typeIndex;
    std::shared_ptr<void> service;
    ServiceLifecycle lifecycle = ServiceLifecycle::UNINITIALIZED;
    std::vector<std::type_index> dependencies;
    std::string name;
    int priority = 0; // Higher priority = initialized first
    
    ServiceMetadata(std::type_index type, std::shared_ptr<void> svc, const std::string& serviceName)
        : typeIndex(type), service(svc), name(serviceName) {}
};

class ServiceLocator {
public:
    static ServiceLocator& instance() {
        static ServiceLocator instance;
        return instance;
    }

    template<typename T>
    void registerService(std::shared_ptr<T> service, const std::string& name = "", int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto typeIndex = std::type_index(typeid(T));
        std::string serviceName = name.empty() ? typeid(T).name() : name;
        
        auto metadata = std::make_unique<ServiceMetadata>(typeIndex, service, serviceName);
        metadata->priority = priority;
        
        services_[typeIndex] = service;
        serviceMetadata_[typeIndex] = std::move(metadata);
        serviceOrder_.push_back(typeIndex);
        
        // Sort by priority for proper initialization order
        std::sort(serviceOrder_.begin(), serviceOrder_.end(), 
                 [this](const std::type_index& a, const std::type_index& b) {
                     return serviceMetadata_[a]->priority > serviceMetadata_[b]->priority;
                 });
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> createAndRegister(const std::string& name = "", int priority = 0, Args&&... args) {
        auto service = std::make_shared<T>(std::forward<Args>(args)...);
        registerService<T>(service, name, priority);
        return service;
    }

    template<typename T>
    std::shared_ptr<T> getService() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto typeIndex = std::type_index(typeid(T));
        auto it = services_.find(typeIndex);
        if (it != services_.end()) {
            auto service = std::static_pointer_cast<T>(it->second);
            
            // Check if service is properly initialized
            auto metaIt = serviceMetadata_.find(typeIndex);
            if (metaIt != serviceMetadata_.end() && 
                metaIt->second->lifecycle != ServiceLifecycle::INITIALIZED) {
                std::cerr << "Warning: Accessing service '" << metaIt->second->name 
                         << "' that is not fully initialized" << std::endl;
            }
            
            return service;
        }
        return nullptr;
    }

    template<typename T>
    T& requireService() {
        auto service = getService<T>();
        if (!service) {
            throw std::runtime_error("Required service not found: " + std::string(typeid(T).name()));
        }
        return *service;
    }

    template<typename T>
    bool hasService() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.find(std::type_index(typeid(T))) != services_.end();
    }

    void unregisterService(const std::type_index& type) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Mark as shutting down
        auto metaIt = serviceMetadata_.find(type);
        if (metaIt != serviceMetadata_.end()) {
            metaIt->second->lifecycle = ServiceLifecycle::SHUTTING_DOWN;
        }
        
        // Remove from order vector
        serviceOrder_.erase(
            std::remove(serviceOrder_.begin(), serviceOrder_.end(), type),
            serviceOrder_.end());
        
        services_.erase(type);
        serviceMetadata_.erase(type);
    }

    template<typename T>
    void unregisterService() {
        unregisterService(std::type_index(typeid(T)));
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Shutdown services in reverse order of initialization
        for (auto it = serviceOrder_.rbegin(); it != serviceOrder_.rend(); ++it) {
            auto metaIt = serviceMetadata_.find(*it);
            if (metaIt != serviceMetadata_.end()) {
                metaIt->second->lifecycle = ServiceLifecycle::SHUTTING_DOWN;
            }
        }
        
        services_.clear();
        serviceMetadata_.clear();
        serviceOrder_.clear();
    }

    // Dependency management
    template<typename T, typename... Dependencies>
    void declareDependencies() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto typeIndex = std::type_index(typeid(T));
        auto metaIt = serviceMetadata_.find(typeIndex);
        if (metaIt != serviceMetadata_.end()) {
            metaIt->second->dependencies = {std::type_index(typeid(Dependencies))...};
        }
    }
    
    // Validate all service dependencies
    bool validateDependencies() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [typeIndex, metadata] : serviceMetadata_) {
            for (const auto& depType : metadata->dependencies) {
                if (services_.find(depType) == services_.end()) {
                    std::cerr << "ERROR: Service '" << metadata->name 
                             << "' depends on missing service: " << depType.name() << std::endl;
                    return false;
                }
            }
        }
        return true;
    }
    
    // Initialize all services in dependency order
    bool initializeAllServices() {
        if (!validateDependencies()) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& typeIndex : serviceOrder_) {
            auto metaIt = serviceMetadata_.find(typeIndex);
            if (metaIt != serviceMetadata_.end()) {
                metaIt->second->lifecycle = ServiceLifecycle::INITIALIZED;
            }
        }
        
        return true;
    }
    
    // Service lifecycle management
    template<typename T>
    void setServiceLifecycle(ServiceLifecycle lifecycle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto typeIndex = std::type_index(typeid(T));
        auto metaIt = serviceMetadata_.find(typeIndex);
        if (metaIt != serviceMetadata_.end()) {
            metaIt->second->lifecycle = lifecycle;
        }
    }
    
    template<typename T>
    ServiceLifecycle getServiceLifecycle() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto typeIndex = std::type_index(typeid(T));
        auto metaIt = serviceMetadata_.find(typeIndex);
        if (metaIt != serviceMetadata_.end()) {
            return metaIt->second->lifecycle;
        }
        return ServiceLifecycle::UNINITIALIZED;
    }
    
    // Debug and introspection
    void printServiceStatus() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== Service Locator Status ===" << std::endl;
        std::cout << "Total Services: " << services_.size() << std::endl;
        
        for (const auto& typeIndex : serviceOrder_) {
            auto metaIt = serviceMetadata_.find(typeIndex);
            if (metaIt != serviceMetadata_.end()) {
                const auto& meta = metaIt->second;
                std::cout << "Service: " << meta->name 
                         << " | Priority: " << meta->priority
                         << " | Lifecycle: " << static_cast<int>(meta->lifecycle)
                         << " | Dependencies: " << meta->dependencies.size() << std::endl;
            }
        }
        std::cout << "=============================" << std::endl;
    }
    
    size_t getServiceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.size();
    }

private:
    ServiceLocator() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
    std::unordered_map<std::type_index, std::unique_ptr<ServiceMetadata>> serviceMetadata_;
    std::vector<std::type_index> serviceOrder_; // Initialization/cleanup order
};

#define DECLARE_SERVICE(ServiceClass) \
    using ServiceTag = void

// Fix missing method - getService should be the correct name
#define IMPLEMENT_SERVICE(ServiceClass) \
    static_assert(Service<ServiceClass>)
