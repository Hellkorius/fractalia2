#pragma once

#include "../core/world_manager.h"
#include "../movement_command_system.h"
#include "../gpu_entity_manager.h"
#include "../components/component.h"
#include <flecs.h>
#include <memory>
#include <vector>

class VulkanContext;
class VulkanSync;
class ResourceContext;

/**
 * @brief Movement and physics module that handles entity movement patterns
 * 
 * This module handles:
 * - Random walk movement calculations
 * - Movement command processing
 * - Entity physics updates
 * - GPU entity synchronization for movement
 * - Integration with movement systems
 */
class MovementModule : public ECSModule {
public:
    explicit MovementModule(GPUEntityManager* gpuManager = nullptr);
    ~MovementModule() override;

    // ECSModule interface
    bool initialize(flecs::world& world) override;
    void shutdown() override;
    void update(float deltaTime) override;
    const std::string& getName() const override;

    // Movement module specific interface
    void setGPUEntityManager(GPUEntityManager* gpuManager);
    GPUEntityManager* getGPUEntityManager() const { return gpuEntityManager_; }

    // Movement command interface
    bool enqueueMovementCommand(const MovementCommand& command);
    void processMovementCommands();
    
    // Movement pattern management
    void updateMovementPatterns(float deltaTime);
    void resetAllMovementPatterns();
    
    // Entity physics
    void updateEntityPhysics(float deltaTime);
    
    // Performance monitoring
    struct MovementStats {
        size_t entitiesWithMovement = 0;
        size_t commandsProcessed = 0;
        size_t commandsEnqueued = 0;
        float lastUpdateTime = 0.0f;
        float averageUpdateTime = 0.0f;
    };
    
    const MovementStats& getStats() const { return stats_; }
    void resetStats();

private:
    static const std::string MODULE_NAME;
    
    flecs::world* world_;
    GPUEntityManager* gpuEntityManager_;
    std::unique_ptr<MovementCommandProcessor> commandProcessor_;
    
    // System entities for proper cleanup
    flecs::entity movementUpdateSystem_;
    flecs::entity physicsUpdateSystem_;
    flecs::entity movementSyncSystem_;
    
    MovementStats stats_;
    
    void registerMovementSystems();
    void setupMovementPhases();
    void cleanupSystems();
    
    // System callbacks
    static void movementUpdateSystemCallback(flecs::entity e, Transform& transform, MovementPattern& pattern);
    static void physicsUpdateSystemCallback(flecs::entity e, Transform& transform, Velocity& velocity);
    static void movementSyncSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, MovementPattern& pattern);
};

/**
 * @brief Static movement system functions for integration with existing systems
 */
namespace MovementSystems {
    /**
     * @brief Update movement patterns for entities
     * @param world The Flecs world
     * @param deltaTime Frame delta time
     */
    void updateMovementPatterns(flecs::world& world, float deltaTime);
    
    /**
     * @brief Update physics for moving entities
     * @param world The Flecs world
     * @param deltaTime Frame delta time
     */
    void updatePhysics(flecs::world& world, float deltaTime);
    
    /**
     * @brief Synchronize movement data with GPU entities
     * @param world The Flecs world
     * @param gpuManager GPU entity manager instance
     */
    void synchronizeWithGPU(flecs::world& world, GPUEntityManager* gpuManager);
}

// Convenience function to get the movement module from WorldManager
namespace MovementModuleAccess {
    /**
     * @brief Get the movement module instance from the world manager
     * @param world The Flecs world instance
     * @return Shared pointer to the movement module, or nullptr if not loaded
     */
    std::shared_ptr<MovementModule> getMovementModule(flecs::world& world);
    
    /**
     * @brief Quick access to enqueue movement commands
     * @param world The Flecs world instance
     * @param command The movement command to enqueue
     * @return True if command was successfully enqueued
     */
    bool enqueueMovementCommand(flecs::world& world, const MovementCommand& command);
    
    /**
     * @brief Process all pending movement commands
     * @param world The Flecs world instance
     */
    void processMovementCommands(flecs::world& world);
}