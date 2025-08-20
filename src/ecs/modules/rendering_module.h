#pragma once

#include "../core/world_manager.h"
#include "../gpu_entity_manager.h"
#include "../components/component.h"
#include <flecs.h>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class VulkanRenderer;
class VulkanContext;
class FrameGraph;

/**
 * @brief Rendering coordination module that manages render preparation systems
 * 
 * This module handles:
 * - Render preparation systems
 * - GPU entity synchronization
 * - Culling and LOD management
 * - Integration with Vulkan renderer
 * - Frame coordination between ECS and rendering
 */
class RenderingModule : public ECSModule {
public:
    explicit RenderingModule(VulkanRenderer* renderer = nullptr, GPUEntityManager* gpuManager = nullptr);
    ~RenderingModule() override;

    // ECSModule interface
    bool initialize(flecs::world& world) override;
    void shutdown() override;
    void update(float deltaTime) override;
    const std::string& getName() const override;

    // Rendering module specific interface
    void setVulkanRenderer(VulkanRenderer* renderer);
    void setGPUEntityManager(GPUEntityManager* gpuManager);
    
    VulkanRenderer* getVulkanRenderer() const { return vulkanRenderer_; }
    GPUEntityManager* getGPUEntityManager() const { return gpuEntityManager_; }

    // Render preparation
    void prepareRenderData(float deltaTime);
    void synchronizeWithGPU();
    void performCulling(const glm::vec3& cameraPosition, const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    
    // LOD management
    void updateLOD(const glm::vec3& cameraPosition);
    void setLODDistances(float nearDistance, float medium, float farDistance);
    
    // Frame coordination
    void beginFrame();
    void endFrame();
    bool shouldRender() const;
    
    // Render state management
    struct RenderState {
        bool cullingEnabled = true;
        bool lodEnabled = true;
        bool frustumCullingEnabled = true;
        float lodNearDistance = 50.0f;
        float lodMediumDistance = 150.0f;
        float lodFarDistance = 300.0f;
        uint32_t maxRenderableEntities = 80000;
    };
    
    void setRenderState(const RenderState& state);
    const RenderState& getRenderState() const { return renderState_; }
    
    // Performance monitoring
    struct RenderingStats {
        size_t totalEntities = 0;
        size_t visibleEntities = 0;
        size_t culledEntities = 0;
        size_t lodLevel0Entities = 0;
        size_t lodLevel1Entities = 0;
        size_t lodLevel2Entities = 0;
        float lastPrepareTime = 0.0f;
        float lastSyncTime = 0.0f;
        float averagePrepareTime = 0.0f;
        float averageSyncTime = 0.0f;
    };
    
    const RenderingStats& getStats() const { return stats_; }
    void resetStats();

    // Camera integration
    void setCameraEntity(flecs::entity cameraEntity);
    flecs::entity getCameraEntity() const { return cameraEntity_; }

private:
    static const std::string MODULE_NAME;
    
    flecs::world* world_;
    VulkanRenderer* vulkanRenderer_;
    GPUEntityManager* gpuEntityManager_;
    
    // System entities for proper cleanup
    flecs::entity renderPrepareSystem_;
    flecs::entity cullSystem_;
    flecs::entity lodSystem_;
    flecs::entity gpuSyncSystem_;
    
    // State management
    RenderState renderState_;
    RenderingStats stats_;
    flecs::entity cameraEntity_;
    bool frameInProgress_;
    
    void registerRenderingSystems();
    void setupRenderingPhases();
    void cleanupSystems();
    
    // System callbacks
    static void renderPrepareSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable);
    static void cullSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, CullingData& cullingData);
    static void lodSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, LODData& lodData);
    static void gpuSyncSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable);
    
    // Helper methods
    bool isEntityVisible(const Transform& transform, const Renderable& renderable, 
                         const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    uint32_t calculateLODLevel(const glm::vec3& entityPosition, const glm::vec3& cameraPosition);
    void updateEntityCullingData(flecs::entity entity, bool visible);
    void updateEntityLODData(flecs::entity entity, uint32_t lodLevel);
};

/**
 * @brief Static rendering system functions for integration with existing systems
 */
namespace RenderingSystems {
    /**
     * @brief Prepare render data for all renderable entities
     * @param world The Flecs world
     * @param deltaTime Frame delta time
     */
    void prepareRenderData(flecs::world& world, float deltaTime);
    
    /**
     * @brief Perform frustum culling on renderable entities
     * @param world The Flecs world
     * @param viewMatrix Camera view matrix
     * @param projMatrix Camera projection matrix
     */
    void performCulling(flecs::world& world, const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    
    /**
     * @brief Update LOD levels for renderable entities
     * @param world The Flecs world
     * @param cameraPosition Camera world position
     * @param lodDistances LOD distance thresholds
     */
    void updateLOD(flecs::world& world, const glm::vec3& cameraPosition, const glm::vec3& lodDistances);
    
    /**
     * @brief Synchronize render data with GPU
     * @param world The Flecs world
     * @param gpuManager GPU entity manager instance
     */
    void synchronizeWithGPU(flecs::world& world, GPUEntityManager* gpuManager);
}

// Convenience function to get the rendering module from WorldManager
namespace RenderingModuleAccess {
    /**
     * @brief Get the rendering module instance from the world manager
     * @param world The Flecs world instance
     * @return Shared pointer to the rendering module, or nullptr if not loaded
     */
    std::shared_ptr<RenderingModule> getRenderingModule(flecs::world& world);
    
    /**
     * @brief Quick access to perform culling
     * @param world The Flecs world instance
     * @param cameraPosition Camera world position
     * @param viewMatrix Camera view matrix
     * @param projMatrix Camera projection matrix
     */
    void performCulling(flecs::world& world, const glm::vec3& cameraPosition, 
                       const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    
    /**
     * @brief Quick access to synchronize with GPU
     * @param world The Flecs world instance
     */
    void synchronizeWithGPU(flecs::world& world);
    
    /**
     * @brief Check if rendering should occur this frame
     * @param world The Flecs world instance
     * @return True if rendering should proceed
     */
    bool shouldRender(flecs::world& world);
}