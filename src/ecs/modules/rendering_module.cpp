#include "rendering_module.h"
#include "../core/service_locator.h"
#include "../../vulkan_renderer.h"
#include <stdexcept>
#include <chrono>
#include <algorithm>

const std::string RenderingModule::MODULE_NAME = "RenderingModule";

RenderingModule::RenderingModule(VulkanRenderer* renderer, GPUEntityManager* gpuManager)
    : vulkanRenderer_(renderer)
    , gpuEntityManager_(gpuManager)
    , world_(nullptr)
    , frameInProgress_(false) {
}

RenderingModule::~RenderingModule() {
    shutdown();
}

bool RenderingModule::initialize(flecs::world& world) {
    if (initialized_) {
        return true;
    }

    try {
        world_ = &world;
        
        // Setup rendering processing phases
        setupRenderingPhases();
        
        // Register rendering systems with proper phases
        registerRenderingSystems();
        
        // Initialize render state
        renderState_ = RenderState{};
        
        // Reset stats
        resetStats();
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
        shutdown();
        return false;
    }
}

void RenderingModule::shutdown() {
    if (!initialized_) {
        return;
    }

    try {
        // End any frame in progress
        if (frameInProgress_) {
            endFrame();
        }
        
        cleanupSystems();
        
        world_ = nullptr;
        vulkanRenderer_ = nullptr;
        gpuEntityManager_ = nullptr;
        cameraEntity_ = flecs::entity();
        
        initialized_ = false;
        
    } catch (const std::exception& e) {
        // Log error but continue shutdown
    }
}

void RenderingModule::update(float deltaTime) {
    if (!initialized_ || !world_) {
        return;
    }

    auto updateStart = std::chrono::high_resolution_clock::now();
    
    try {
        // Prepare render data
        prepareRenderData(deltaTime);
        
        // Update performance stats
        auto updateEnd = std::chrono::high_resolution_clock::now();
        auto updateDuration = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
        
        stats_.lastPrepareTime = updateDuration;
        stats_.averagePrepareTime = (stats_.averagePrepareTime * 0.95f) + (updateDuration * 0.05f);
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
    }
}

const std::string& RenderingModule::getName() const {
    return MODULE_NAME;
}

void RenderingModule::setVulkanRenderer(VulkanRenderer* renderer) {
    vulkanRenderer_ = renderer;
}

void RenderingModule::setGPUEntityManager(GPUEntityManager* gpuManager) {
    gpuEntityManager_ = gpuManager;
}

void RenderingModule::prepareRenderData(float deltaTime) {
    if (!world_) {
        return;
    }
    
    // Use the static system function for consistency
    RenderingSystems::prepareRenderData(*world_, deltaTime);
    
    // Update entity counts
    stats_.totalEntities = 0;
    world_->query<Transform, Renderable>().each([this](flecs::entity e, Transform& transform, Renderable& renderable) {
        stats_.totalEntities++;
    });
}

void RenderingModule::synchronizeWithGPU() {
    if (!world_ || !gpuEntityManager_) {
        return;
    }
    
    auto syncStart = std::chrono::high_resolution_clock::now();
    
    try {
        // Use the static system function for consistency
        RenderingSystems::synchronizeWithGPU(*world_, gpuEntityManager_);
        
        // Update sync timing stats
        auto syncEnd = std::chrono::high_resolution_clock::now();
        auto syncDuration = std::chrono::duration<float, std::milli>(syncEnd - syncStart).count();
        
        stats_.lastSyncTime = syncDuration;
        stats_.averageSyncTime = (stats_.averageSyncTime * 0.95f) + (syncDuration * 0.05f);
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
    }
}

void RenderingModule::performCulling(const glm::vec3& cameraPosition, const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
    if (!world_ || !renderState_.cullingEnabled) {
        return;
    }
    
    // Reset culling stats
    stats_.visibleEntities = 0;
    stats_.culledEntities = 0;
    
    // Use the static system function for consistency
    RenderingSystems::performCulling(*world_, viewMatrix, projMatrix);
    
    // Count visible entities
    world_->query<CullingData>().each([this](flecs::entity e, CullingData& cullingData) {
        if (cullingData.visible) {
            stats_.visibleEntities++;
        } else {
            stats_.culledEntities++;
        }
    });
}

void RenderingModule::updateLOD(const glm::vec3& cameraPosition) {
    if (!world_ || !renderState_.lodEnabled) {
        return;
    }
    
    // Reset LOD stats
    stats_.lodLevel0Entities = 0;
    stats_.lodLevel1Entities = 0;
    stats_.lodLevel2Entities = 0;
    
    glm::vec3 lodDistances(renderState_.lodNearDistance, renderState_.lodMediumDistance, renderState_.lodFarDistance);
    
    // Use the static system function for consistency
    RenderingSystems::updateLOD(*world_, cameraPosition, lodDistances);
    
    // Count LOD entities
    world_->query<LODData>().each([this](flecs::entity e, LODData& lodData) {
        switch (lodData.level) {
            case 0: stats_.lodLevel0Entities++; break;
            case 1: stats_.lodLevel1Entities++; break;
            case 2: stats_.lodLevel2Entities++; break;
        }
    });
}

void RenderingModule::setLODDistances(float nearDistance, float medium, float farDistance) {
    renderState_.lodNearDistance = nearDistance;
    renderState_.lodMediumDistance = medium;
    renderState_.lodFarDistance = farDistance;
}

void RenderingModule::beginFrame() {
    frameInProgress_ = true;
}

void RenderingModule::endFrame() {
    frameInProgress_ = false;
}

bool RenderingModule::shouldRender() const {
    return initialized_ && vulkanRenderer_ && gpuEntityManager_;
}

void RenderingModule::setRenderState(const RenderState& state) {
    renderState_ = state;
}

void RenderingModule::resetStats() {
    stats_ = {};
}

void RenderingModule::setCameraEntity(flecs::entity cameraEntity) {
    cameraEntity_ = cameraEntity;
}

void RenderingModule::setupRenderingPhases() {
    // Create render preparation phase that runs after movement
    auto renderPreparePhase = world_->entity("RenderPreparePhase")
        .add(flecs::Phase)
        .depends_on(flecs::OnUpdate);
        
    // Create culling phase that runs after render preparation
    auto cullPhase = world_->entity("CullPhase")
        .add(flecs::Phase)
        .depends_on(renderPreparePhase);
        
    // Create LOD phase that runs after culling
    auto lodPhase = world_->entity("LODPhase")
        .add(flecs::Phase)
        .depends_on(cullPhase);
        
    // Create GPU sync phase that runs last
    auto gpuSyncPhase = world_->entity("GPUSyncPhase")
        .add(flecs::Phase)
        .depends_on(lodPhase);
}

void RenderingModule::registerRenderingSystems() {
    auto renderPreparePhase = world_->entity("RenderPreparePhase");
    auto cullPhase = world_->entity("CullPhase");
    auto lodPhase = world_->entity("LODPhase");
    auto gpuSyncPhase = world_->entity("GPUSyncPhase");
    
    // Register render preparation system
    renderPrepareSystem_ = world_->system<Transform, Renderable>()
        .kind(renderPreparePhase)
        .each(renderPrepareSystemCallback);
        
    // Register culling system
    cullSystem_ = world_->system<Transform, Renderable, CullingData>()
        .kind(cullPhase)
        .each(cullSystemCallback);
        
    // Register LOD system
    lodSystem_ = world_->system<Transform, Renderable, LODData>()
        .kind(lodPhase)
        .each(lodSystemCallback);
        
    // Register GPU synchronization system
    gpuSyncSystem_ = world_->system<Transform, Renderable>()
        .kind(gpuSyncPhase)
        .each(gpuSyncSystemCallback);
        
    if (!renderPrepareSystem_.is_valid() || !cullSystem_.is_valid() || 
        !lodSystem_.is_valid() || !gpuSyncSystem_.is_valid()) {
        throw std::runtime_error("Failed to register rendering systems");
    }
}

void RenderingModule::cleanupSystems() {
    if (renderPrepareSystem_.is_valid()) {
        renderPrepareSystem_.destruct();
    }
    if (cullSystem_.is_valid()) {
        cullSystem_.destruct();
    }
    if (lodSystem_.is_valid()) {
        lodSystem_.destruct();
    }
    if (gpuSyncSystem_.is_valid()) {
        gpuSyncSystem_.destruct();
    }
}

// System callback implementations
void RenderingModule::renderPrepareSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable) {
    // Update model matrix
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), transform.position);
    glm::mat4 rotationMatrix = glm::mat4_cast(transform.rotation);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);
    
    renderable.modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    
    // Mark as dirty for GPU sync
    renderable.dirty = true;
}

void RenderingModule::cullSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, CullingData& cullingData) {
    // In a full implementation, this would perform frustum culling
    // For now, we'll just mark everything as visible
    cullingData.visible = true;
    cullingData.distance = glm::length(transform.position); // Simple distance calculation
}

void RenderingModule::lodSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, LODData& lodData) {
    // Simple LOD calculation based on distance
    float distance = glm::length(transform.position);
    
    if (distance < 50.0f) {
        lodData.level = 0; // High detail
    } else if (distance < 150.0f) {
        lodData.level = 1; // Medium detail
    } else {
        lodData.level = 2; // Low detail
    }
    
    lodData.distance = distance;
}

void RenderingModule::gpuSyncSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable) {
    // This system marks entities that need GPU synchronization
    // The actual sync happens in the module's synchronizeWithGPU method
    if (renderable.dirty) {
        // Mark for GPU sync (this would typically add to a sync queue)
        renderable.dirty = false;
    }
}

// Helper method implementations
bool RenderingModule::isEntityVisible(const Transform& transform, const Renderable& renderable, 
                                     const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
    // Simple visibility check - in production this would do proper frustum culling
    return true;
}

uint32_t RenderingModule::calculateLODLevel(const glm::vec3& entityPosition, const glm::vec3& cameraPosition) {
    float distance = glm::length(entityPosition - cameraPosition);
    
    if (distance < renderState_.lodNearDistance) {
        return 0;
    } else if (distance < renderState_.lodMediumDistance) {
        return 1;
    } else {
        return 2;
    }
}

void RenderingModule::updateEntityCullingData(flecs::entity entity, bool visible) {
    if (entity.has<CullingData>()) {
        auto cullingData = entity.get_mut<CullingData>();
        cullingData->visible = visible;
    }
}

void RenderingModule::updateEntityLODData(flecs::entity entity, uint32_t lodLevel) {
    if (entity.has<LODData>()) {
        auto lodData = entity.get_mut<LODData>();
        lodData->level = lodLevel;
    }
}

// RenderingSystems namespace implementation
namespace RenderingSystems {
    void prepareRenderData(flecs::world& world, float deltaTime) {
        world.query<Transform, Renderable>().each([](flecs::entity e, Transform& transform, Renderable& renderable) {
            // Update model matrix
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), transform.position);
            glm::mat4 rotationMatrix = glm::mat4_cast(transform.rotation);
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);
            
            renderable.modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
            renderable.dirty = true;
        });
    }
    
    void performCulling(flecs::world& world, const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
        world.query<Transform, Renderable>().each([&](flecs::entity e, Transform& transform, Renderable& renderable) {
            // Ensure entity has culling data
            if (!e.has<CullingData>()) {
                e.add<CullingData>();
            }
            
            auto cullingData = e.get_mut<CullingData>();
            cullingData->visible = true; // Simple implementation - mark all visible
            cullingData->distance = glm::length(transform.position);
        });
    }
    
    void updateLOD(flecs::world& world, const glm::vec3& cameraPosition, const glm::vec3& lodDistances) {
        world.query<Transform, Renderable>().each([&](flecs::entity e, Transform& transform, Renderable& renderable) {
            // Ensure entity has LOD data
            if (!e.has<LODData>()) {
                e.add<LODData>();
            }
            
            auto lodData = e.get_mut<LODData>();
            float distance = glm::length(transform.position - cameraPosition);
            
            if (distance < lodDistances.x) {
                lodData->level = 0;
            } else if (distance < lodDistances.y) {
                lodData->level = 1;
            } else {
                lodData->level = 2;
            }
            
            lodData->distance = distance;
        });
    }
    
    void synchronizeWithGPU(flecs::world& world, GPUEntityManager* gpuManager) {
        if (!gpuManager) {
            return;
        }
        
        std::vector<flecs::entity> entities;
        world.query<Transform, Renderable>().each([&entities](flecs::entity e, Transform& transform, Renderable& renderable) {
            if (renderable.dirty) {
                entities.push_back(e);
                renderable.dirty = false;
            }
        });
        
        if (!entities.empty()) {
            gpuManager->addEntitiesFromECS(entities);
            gpuManager->uploadPendingEntities();
        }
    }
}

// RenderingModuleAccess namespace implementation
namespace RenderingModuleAccess {
    std::shared_ptr<RenderingModule> getRenderingModule(flecs::world& world) {
        auto worldManager = ServiceLocator::instance().getService<WorldManager>();
        if (!worldManager) {
            return nullptr;
        }
        
        return worldManager->getModule<RenderingModule>("RenderingModule");
    }
    
    void performCulling(flecs::world& world, const glm::vec3& cameraPosition, 
                       const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
        auto module = getRenderingModule(world);
        if (module) {
            module->performCulling(cameraPosition, viewMatrix, projMatrix);
        }
    }
    
    void synchronizeWithGPU(flecs::world& world) {
        auto module = getRenderingModule(world);
        if (module) {
            module->synchronizeWithGPU();
        }
    }
    
    bool shouldRender(flecs::world& world) {
        auto module = getRenderingModule(world);
        return module ? module->shouldRender() : false;
    }
}