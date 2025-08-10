#pragma once

#include <flecs.h>
#include "entity.hpp"
#include "component.hpp"
#include "entity_factory.hpp"
#include "memory_manager.hpp"
#include "system_scheduler.hpp"
#include <unordered_map>
#include <memory>
#include <chrono>

// Query cache for performance optimization
class QueryCache {
private:
    struct CachedQuery {
        std::shared_ptr<void> query;
        std::chrono::steady_clock::time_point lastUsed;
        size_t useCount{0};
        
        template<typename... Components>
        flecs::query<Components...>* getQuery() {
            return static_cast<flecs::query<Components...>*>(query.get());
        }
    };
    
    std::unordered_map<std::string, CachedQuery> cache;
    std::chrono::seconds maxAge{300}; // 5 minutes
    size_t maxSize{100};
    
public:
    template<typename... Components>
    flecs::query<Components...>& getOrCreateQuery(flecs::world& world, const std::string& key) {
        auto it = cache.find(key);
        
        if (it != cache.end()) {
            it->second.lastUsed = std::chrono::steady_clock::now();
            it->second.useCount++;
            return *it->second.getQuery<Components...>();
        }
        
        // Create new query
        auto query = std::make_shared<flecs::query<Components...>>(world.query<Components...>());
        
        CachedQuery cached;
        cached.query = query;
        cached.lastUsed = std::chrono::steady_clock::now();
        cached.useCount = 1;
        
        cache[key] = cached;
        
        // Cleanup if cache is too large
        if (cache.size() > maxSize) {
            cleanup();
        }
        
        return *static_cast<flecs::query<Components...>*>(cached.query.get());
    }
    
    void cleanup() {
        auto now = std::chrono::steady_clock::now();
        
        auto it = cache.begin();
        while (it != cache.end()) {
            if ((now - it->second.lastUsed) > maxAge) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    size_t size() const { return cache.size(); }
};

class World {
private:
    flecs::world flecsWorld;
    std::unique_ptr<EntityFactory> entityFactory;
    std::unique_ptr<ECSMemoryManager> memoryManager;
    std::unique_ptr<QueryCache> queryCache;
    std::unique_ptr<SystemScheduler> systemScheduler;
    
    // Performance tracking
    std::chrono::steady_clock::time_point lastUpdateTime;
    float deltaTime{0.0f};
    size_t frameCount{0};

public:
    World() : lastUpdateTime(std::chrono::steady_clock::now()) {
        entityFactory = std::make_unique<EntityFactory>(flecsWorld);
        memoryManager = std::make_unique<ECSMemoryManager>(flecsWorld);
        queryCache = std::make_unique<QueryCache>();
        systemScheduler = std::make_unique<SystemScheduler>(flecsWorld);
    }
    
    ~World() = default;

    // Enhanced entity creation through factory
    EntityBuilder createEntity() {
        return entityFactory->create();
    }
    
    Entity entity() {
        return flecsWorld.entity();
    }

    template<typename... Components>
    auto system() {
        return flecsWorld.system<Components...>();
    }

    // Cached query access for better performance
    template<typename... Components>
    auto query(const std::string& key = "") {
        if (key.empty()) {
            return flecsWorld.query<Components...>();
        }
        return queryCache->getOrCreateQuery<Components...>(flecsWorld, key);
    }

    void progress(float dt) {
        // Update timing
        auto now = std::chrono::steady_clock::now();
        if (frameCount > 0) {
            deltaTime = std::chrono::duration<float>(now - lastUpdateTime).count();
        }
        lastUpdateTime = now;
        frameCount++;
        
        // Use system scheduler for proper system execution
        systemScheduler->executeFrame(dt);
        
        // Periodic cleanup
        if (frameCount % 300 == 0) { // Every 5 seconds at 60fps
            cleanup();
        }
    }
    
    // Factory access
    EntityFactory& getEntityFactory() { return *entityFactory; }
    
    // Memory management
    ECSMemoryManager& getMemoryManager() { return *memoryManager; }
    
    // System scheduler access
    SystemScheduler& getSystemScheduler() { return *systemScheduler; }
    
    // Direct access to flecs world for advanced usage
    flecs::world& getFlecsWorld() {
        return flecsWorld;
    }

    const flecs::world& getFlecsWorld() const {
        return flecsWorld;
    }
    
    // Performance and cleanup
    void cleanup() {
        memoryManager->cleanup();
        queryCache->cleanup();
    }
    
    float getDeltaTime() const { return deltaTime; }
    size_t getFrameCount() const { return frameCount; }
    
    // Statistics
    struct WorldStats {
        size_t frameCount;
        float deltaTime;
        size_t cachedQueries;
        ECSMemoryManager::MemoryStats memoryStats;
    };
    
    WorldStats getStats() {
        // Ensure memory stats are up to date
        memoryManager->updateStats();
        auto memStats = memoryManager->getStats();
        
        return {
            frameCount,
            deltaTime,
            queryCache->size(),
            memStats
        };
    }
};