#include "rendering_service.h"
#include "camera_service.h"
#include "../core/service_locator.h"
#include "../gpu/gpu_entity_manager.h"
#include "../../vulkan_renderer.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <execution>
#include <thread>
#include <stdexcept>

RenderingService::RenderingService() {
}

RenderingService::~RenderingService() {
    cleanup();
}

bool RenderingService::initialize(flecs::world& world, VulkanRenderer* renderer) {
    if (initialized) {
        return true;
    }
    
    if (!renderer) {
        std::cerr << "RenderingService: Invalid renderer provided" << std::endl;
        return false;
    }
    
    this->world = &world;
    this->renderer = renderer;
    this->gpuEntityManager = renderer->getGPUEntityManager();
    
    if (!gpuEntityManager) {
        std::cerr << "RenderingService: Failed to get GPU entity manager from renderer" << std::endl;
        return false;
    }
    
    // Reserve space for render queue
    renderQueue.reserve(maxRenderableEntities);
    renderBatches.reserve(100); // Reasonable default for batches
    
    // Cache service dependencies
    if (ServiceLocator::instance().hasService<CameraService>()) {
        cameraService = &ServiceLocator::instance().requireService<CameraService>();
    }
    
    // Setup ECS integration (from RenderingModule)
    try {
        setupRenderingPhases();
        registerRenderingSystems();
        
        // Initialize render state
        renderState_ = RenderState{};
    } catch (const std::exception& e) {
        std::cerr << "RenderingService: Failed to setup ECS integration: " << e.what() << std::endl;
        cleanup();
        return false;
    }
    
    // Reset statistics
    resetStats();
    
    initialized = true;
    return true;
}

void RenderingService::cleanup() {
    if (!initialized) {
        return;
    }
    
    try {
        // End any frame in progress
        if (frameInProgress_) {
            endFrame();
        }
        
        // Cleanup ECS systems
        cleanupSystems();
    } catch (const std::exception& e) {
        // Log error but continue cleanup
        std::cerr << "RenderingService: Error during cleanup: " << e.what() << std::endl;
    }
    
    clearRenderQueue();
    renderBatches.clear();
    entityToQueueIndex.clear();
    
    world = nullptr;
    renderer = nullptr;
    gpuEntityManager = nullptr;
    cameraService = nullptr;
    cameraEntity_ = flecs::entity();
    
    initialized = false;
}

void RenderingService::processFrame(float deltaTime) {
    if (!initialized) {
        return;
    }
    
    this->deltaTime = deltaTime;
    frameNumber++;
    
    // Start timing
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Reset frame statistics
    cullingStats.reset();
    renderStats.reset();
    
    // Update from ECS
    updateFromECS();
    
    // Build render queue
    buildRenderQueue();
    
    // Perform culling
    if (frustumCullingEnabled || occlusionCullingEnabled) {
        performCulling();
    }
    
    // Sort render queue
    sortRenderQueue();
    
    // Create batches if enabled
    if (batchingEnabled) {
        createRenderBatches();
    }
    
    // Execute pre-render callback
    if (preRenderCallback) {
        preRenderCallback(renderQueue);
    }
    
    // Prepare GPU data
    prepareGPUData();
    
    // Calculate timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    renderStats.cpuRenderTimeMs = duration.count() / 1000.0f;
    
    // Update statistics
    updateCullingStats();
    updateRenderStats();
    
    // Execute post-render callback
    if (postRenderCallback) {
        postRenderCallback(renderStats);
    }
}

void RenderingService::beginFrame() {
    if (!initialized) {
        return;
    }
    
    frameInProgress_ = true;
    
    // Clear previous frame data
    clearRenderQueue();
    
    // Reset frame-specific state
    cullingStats.reset();
    renderStats.reset();
}

void RenderingService::endFrame() {
    if (!initialized) {
        return;
    }
    
    frameInProgress_ = false;
    
    // Finalize statistics
    updateRenderStats();
}

void RenderingService::registerRenderableEntity(flecs::entity entity) {
    if (!initialized || !entity.is_valid()) {
        return;
    }
    
    // Check if entity has required components
    if (!entity.has<Transform>() || !entity.has<Renderable>()) {
        return;
    }
    
    // Entity will be picked up automatically in the next frame's ECS query
    // No need to maintain a separate registry since we query ECS directly
}

void RenderingService::unregisterRenderableEntity(flecs::entity entity) {
    if (!initialized || !entity.is_valid()) {
        return;
    }
    
    // Remove from entity to queue index mapping
    entityToQueueIndex.erase(entity);
    
    // Entity will be automatically excluded from queries once components are removed
}

void RenderingService::updateEntityData(flecs::entity entity, const Transform& transform, const Renderable& renderable) {
    if (!initialized || !entity.is_valid()) {
        return;
    }
    
    // Update the entity's components directly in ECS
    entity.set<Transform>(transform);
    entity.set<Renderable>(renderable);
    
    // Mark entity as dirty for GPU upload
    markEntityDirty(entity);
}

void RenderingService::markEntityDirty(flecs::entity entity) {
    if (!initialized || !entity.is_valid()) {
        return;
    }
    
    // Add GPU upload pending marker
    if (!entity.has<GPUUploadPending>()) {
        entity.add<GPUUploadPending>();
    }
}

void RenderingService::buildRenderQueue() {
    if (!initialized) {
        return;
    }
    
    clearRenderQueue();
    collectRenderableEntities();
    
    cullingStats.totalEntities = renderQueue.size();
}

void RenderingService::sortRenderQueue() {
    if (!initialized || renderQueue.empty()) {
        return;
    }
    
    // Generate sort keys for all entries
    for (auto& entry : renderQueue) {
        entry.generateSortKey();
    }
    
    if (multithreadingEnabled && renderQueue.size() > 1000) {
        // Use parallel sort for large queues
        std::sort(std::execution::par_unseq, renderQueue.begin(), renderQueue.end(),
                 [](const RenderQueueEntry& a, const RenderQueueEntry& b) {
                     return a.sortKey < b.sortKey;
                 });
    } else {
        // Use sequential sort for smaller queues
        std::sort(renderQueue.begin(), renderQueue.end(),
                 [](const RenderQueueEntry& a, const RenderQueueEntry& b) {
                     return a.sortKey < b.sortKey;
                 });
    }
}

void RenderingService::submitRenderQueue() {
    if (!initialized) {
        return;
    }
    
    if (batchingEnabled) {
        // Submit batches
        for (const auto& batch : renderBatches) {
            if (batch.instanceCount > 0) {
                submitBatch(batch);
            }
        }
    } else {
        // Submit individual entries
        for (const auto& entry : renderQueue) {
            if (entry.visible) {
                // Individual submission would be handled by renderer
                renderStats.totalDrawCalls++;
                renderStats.totalInstances++;
            }
        }
    }
}

void RenderingService::clearRenderQueue() {
    renderQueue.clear();
    renderBatches.clear();
    entityToQueueIndex.clear();
}

void RenderingService::performCulling() {
    if (!initialized || renderQueue.empty()) {
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Use cached camera service for frustum culling
    
    uint32_t visibleCount = 0;
    uint32_t frustumCulledCount = 0;
    uint32_t occlusionCulledCount = 0;
    uint32_t lodCulledCount = 0;
    
    for (auto& entry : renderQueue) {
        bool visible = true;
        
        // Distance culling
        if (entry.distanceToCamera > maxRenderDistance) {
            visible = false;
        }
        
        // Frustum culling
        if (visible && frustumCullingEnabled && cameraService) {
            // Get bounds from entity if available
            Bounds bounds;
            if (entry.entity.has<Bounds>()) {
                bounds = *entry.entity.get<Bounds>();
            } else {
                // Use default bounds
                bounds.min = glm::vec3(-0.5f);
                bounds.max = glm::vec3(0.5f);
            }
            
            if (!cameraService->isEntityVisible(entry.transform, bounds)) {
                visible = false;
                frustumCulledCount++;
            }
        }
        
        // Occlusion culling (placeholder - requires occlusion query system)
        if (visible && occlusionCullingEnabled) {
            // TODO: Implement occlusion culling
            // For now, just a placeholder
            if (performOcclusionCulling(entry.transform, Bounds{})) {
                visible = false;
                occlusionCulledCount++;
            }
        }
        
        // LOD culling (cull entities with too high LOD level)
        if (visible && lodConfig.enabled) {
            if (entry.lodLevel >= static_cast<int>(lodConfig.distances.size())) {
                visible = false;
                lodCulledCount++;
            }
        }
        
        entry.visible = visible;
        if (visible) {
            visibleCount++;
        }
    }
    
    // Update culling statistics
    cullingStats.visibleEntities = visibleCount;
    cullingStats.frustumCulled = frustumCulledCount;
    cullingStats.occlusionCulled = occlusionCulledCount;
    cullingStats.lodCulled = lodCulledCount;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    cullingStats.cullingTimeMs = duration.count() / 1000.0f;
}

int RenderingService::calculateLOD(const glm::vec3& entityPosition, const glm::vec3& cameraPosition) const {
    if (!lodConfig.enabled) {
        return 0;
    }
    
    float distance = glm::length(entityPosition - cameraPosition);
    distance += lodConfig.lodBias;
    
    for (size_t i = 0; i < lodConfig.distances.size(); ++i) {
        if (distance <= lodConfig.distances[i]) {
            return static_cast<int>(i);
        }
    }
    
    return static_cast<int>(lodConfig.distances.size());
}

void RenderingService::createRenderBatches() {
    if (!initialized || renderQueue.empty()) {
        return;
    }
    
    renderBatches.clear();
    
    RenderBatch currentBatch;
    currentBatch.priority = RenderPriority::NORMAL;
    
    for (const auto& entry : renderQueue) {
        if (!entry.visible) {
            continue;
        }
        
        // Check if we can batch with the current batch
        bool canBatch = currentBatch.entries.empty() || 
                       (currentBatch.priority == entry.priority && 
                        currentBatch.entries.back().lodLevel == entry.lodLevel);
        
        if (!canBatch && !currentBatch.entries.empty()) {
            // Finalize current batch
            currentBatch.instanceCount = currentBatch.entries.size();
            renderBatches.push_back(std::move(currentBatch));
            currentBatch = RenderBatch();
            currentBatch.priority = entry.priority;
        }
        
        currentBatch.entries.push_back(entry);
    }
    
    // Add the final batch if it has entries
    if (!currentBatch.entries.empty()) {
        currentBatch.instanceCount = currentBatch.entries.size();
        renderBatches.push_back(std::move(currentBatch));
    }
    
    cullingStats.renderQueueSize = renderBatches.size();
}

void RenderingService::submitBatch(const RenderBatch& batch) {
    if (!initialized || batch.entries.empty()) {
        return;
    }
    
    // This would interface with the actual renderer
    // For now, just update statistics
    renderStats.batchesSubmitted++;
    renderStats.totalDrawCalls++; // One draw call per batch
    renderStats.totalInstances += batch.instanceCount;
    
    // In a real implementation, this would:
    // 1. Set appropriate pipeline state
    // 2. Bind descriptor sets
    // 3. Issue instanced draw call
}

void RenderingService::resetStats() {
    cullingStats.reset();
    renderStats.reset();
}

void RenderingService::printStats() const {
    std::cout << "=== Rendering Service Statistics ===" << std::endl;
    std::cout << "Frame: " << frameNumber << std::endl;
    
    std::cout << "\n--- Culling Stats ---" << std::endl;
    std::cout << "Total Entities: " << cullingStats.totalEntities << std::endl;
    std::cout << "Visible Entities: " << cullingStats.visibleEntities << std::endl;
    std::cout << "Frustum Culled: " << cullingStats.frustumCulled << std::endl;
    std::cout << "Occlusion Culled: " << cullingStats.occlusionCulled << std::endl;
    std::cout << "LOD Culled: " << cullingStats.lodCulled << std::endl;
    std::cout << "Culling Ratio: " << (cullingStats.getCullingRatio() * 100.0f) << "%" << std::endl;
    std::cout << "Culling Time: " << cullingStats.cullingTimeMs << "ms" << std::endl;
    
    std::cout << "\n--- Render Stats ---" << std::endl;
    std::cout << "Draw Calls: " << renderStats.totalDrawCalls << std::endl;
    std::cout << "Batches: " << renderStats.batchesSubmitted << std::endl;
    std::cout << "Instances: " << renderStats.totalInstances << std::endl;
    std::cout << "CPU Render Time: " << renderStats.cpuRenderTimeMs << "ms" << std::endl;
    std::cout << "GPU Render Time: " << renderStats.gpuRenderTimeMs << "ms" << std::endl;
}

void RenderingService::drawDebugInfo() {
    if (!debugVisualization || !initialized) {
        return;
    }
    
    // This would render debug visualization
    // For now, just print debug info
    printStats();
}

void RenderingService::syncWithGPUEntityManager() {
    if (!initialized || !gpuEntityManager) {
        return;
    }
    
    // Sync any pending uploads
    if (gpuEntityManager->hasPendingUploads()) {
        gpuEntityManager->uploadPendingEntities();
    }
}

void RenderingService::updateFromECS() {
    if (!initialized || !world) {
        return;
    }
    
    // Query for entities that need GPU upload
    world->query<Transform, Renderable>().each([this](flecs::entity entity, 
                                                      Transform& transform, 
                                                      Renderable& renderable) {
        // Check if entity has GPU upload pending tag
        if (!entity.has<GPUUploadPending>()) {
            return;
        }
        // Create GPU entity and add to manager
        GPUEntity gpuEntity = GPUEntity::fromECS(transform, renderable, MovementPattern{});
        gpuEntityManager->addEntity(gpuEntity);
        
        // Remove the pending marker
        entity.remove<GPUUploadPending>();
        entity.add<GPUUploadComplete>();
    });
}

void RenderingService::prepareGPUData() {
    if (!initialized) {
        return;
    }
    
    // Sync with GPU entity manager
    syncWithGPUEntityManager();
    
    // Upload any pending entity data to GPU
    if (gpuEntityManager->hasPendingUploads()) {
        gpuEntityManager->uploadPendingEntities();
    }
}

void RenderingService::processRenderingMT() {
    if (!multithreadingEnabled || !initialized) {
        processFrame(deltaTime);
        return;
    }
    
    // Multi-threaded processing
    std::vector<std::thread> workers;
    
    // Thread 1: ECS updates
    workers.emplace_back([this]() {
        updateFromECS();
    });
    
    // Thread 2: Build render queue
    workers.emplace_back([this]() {
        buildRenderQueue();
    });
    
    // Wait for completion
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // Single-threaded operations that depend on the above
    if (frustumCullingEnabled || occlusionCullingEnabled) {
        performCulling();
    }
    
    sortRenderQueue();
    
    if (batchingEnabled) {
        createRenderBatches();
    }
    
    prepareGPUData();
}

void RenderingService::renderViewport(const std::string& viewportName) {
    // Placeholder for viewport-specific rendering
    // This would coordinate with CameraService to render from specific camera/viewport
    activeViewportName = viewportName;
    processFrame(deltaTime);
}

void RenderingService::renderAllViewports() {
    // Get all active viewports from CameraService and render each one
    if (cameraService) {
        auto activeViewports = cameraService->getActiveViewports();
        
        for (const auto* viewport : activeViewports) {
            renderViewport(viewport->name);
        }
    }
}

// Private helper methods
void RenderingService::collectRenderableEntities() {
    if (!initialized || !world) {
        return;
    }
    
    // Get camera position for distance calculations
    glm::vec3 cameraPosition(0.0f);
    if (cameraService) {
        cameraPosition = cameraService->getCameraPosition();
    }
    
    // Query all renderable entities
    world->query<Transform, Renderable>().each([this, cameraPosition](flecs::entity entity, 
                                                              const Transform& transform, 
                                                              const Renderable& renderable) {
        if (!renderable.visible) {
            return; // Skip invisible entities
        }
        
        RenderQueueEntry entry = createQueueEntry(entity, transform, renderable);
        entry.distanceToCamera = glm::length(transform.position - cameraPosition);
        entry.lodLevel = calculateLOD(transform.position, cameraPosition);
        
        renderQueue.push_back(entry);
        entityToQueueIndex[entity] = renderQueue.size() - 1;
    });
}

bool RenderingService::isEntityVisible(const RenderQueueEntry& entry) const {
    return entry.visible && entry.distanceToCamera <= maxRenderDistance;
}

bool RenderingService::performFrustumCulling(const Transform& transform, const Bounds& bounds) const {
    // This would perform actual frustum culling
    // For now, return true (visible)
    return true;
}

bool RenderingService::performOcclusionCulling(const Transform& transform, const Bounds& bounds) const {
    // This would perform occlusion culling using GPU queries
    // For now, return false (not occluded)
    return false;
}

void RenderingService::updateCullingStats() {
    // Statistics are updated during the culling process
}

void RenderingService::updateRenderStats() {
    // Update any additional render statistics
    renderStats.totalTriangles = renderStats.totalInstances * 2; // Assuming quads
    renderStats.totalVertices = renderStats.totalInstances * 4;  // 4 vertices per quad
}

RenderQueueEntry RenderingService::createQueueEntry(flecs::entity entity, const Transform& transform, const Renderable& renderable) {
    RenderQueueEntry entry;
    entry.entity = entity;
    entry.transform = transform;
    entry.renderable = renderable;
    entry.priority = static_cast<RenderPriority>(renderable.layer);
    entry.visible = true;
    entry.distanceToCamera = 0.0f;
    entry.lodLevel = 0;
    entry.sortKey = 0;
    
    return entry;
}

bool RenderingService::canBatchTogether(const RenderQueueEntry& a, const RenderQueueEntry& b) const {
    return a.priority == b.priority && a.lodLevel == b.lodLevel;
}

void RenderingService::waitForGPUIdle() {
    // This would wait for GPU to complete all operations
    // Implementation would depend on the Vulkan renderer
}

void RenderingService::flushPendingOperations() {
    // Flush any pending GPU operations
    syncWithGPUEntityManager();
}

// ============================================================================
// ECS Integration Methods (absorbed from RenderingModule)
// ============================================================================

void RenderingService::setupRenderingPhases() {
    if (!world) {
        throw std::runtime_error("World not initialized");
    }
    
    // Create render preparation phase that runs after movement
    auto renderPreparePhase = world->entity("RenderPreparePhase")
        .add(flecs::Phase)
        .depends_on(flecs::OnUpdate);
        
    // Create culling phase that runs after render preparation
    auto cullPhase = world->entity("CullPhase")
        .add(flecs::Phase)
        .depends_on(renderPreparePhase);
        
    // Create LOD phase that runs after culling
    auto lodPhase = world->entity("LODPhase")
        .add(flecs::Phase)
        .depends_on(cullPhase);
        
    // Create GPU sync phase that runs last
    auto gpuSyncPhase = world->entity("GPUSyncPhase")
        .add(flecs::Phase)
        .depends_on(lodPhase);
}

void RenderingService::registerRenderingSystems() {
    if (!world) {
        throw std::runtime_error("World not initialized");
    }
    
    auto renderPreparePhase = world->entity("RenderPreparePhase");
    auto cullPhase = world->entity("CullPhase");
    auto lodPhase = world->entity("LODPhase");
    auto gpuSyncPhase = world->entity("GPUSyncPhase");
    
    // Register render preparation system
    renderPrepareSystem_ = world->system<Transform, Renderable>()
        .kind(renderPreparePhase)
        .each(renderPrepareSystemCallback);
        
    // Register culling system
    cullSystem_ = world->system<Transform, Renderable, CullingData>()
        .kind(cullPhase)
        .each(cullSystemCallback);
        
    // Register LOD system
    lodSystem_ = world->system<Transform, Renderable, LODData>()
        .kind(lodPhase)
        .each(lodSystemCallback);
        
    // Register GPU synchronization system
    gpuSyncSystem_ = world->system<Transform, Renderable>()
        .kind(gpuSyncPhase)
        .each(gpuSyncSystemCallback);
        
    if (!renderPrepareSystem_.is_valid() || !cullSystem_.is_valid() || 
        !lodSystem_.is_valid() || !gpuSyncSystem_.is_valid()) {
        throw std::runtime_error("Failed to register rendering systems");
    }
}

void RenderingService::cleanupSystems() {
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

// beginFrame() and endFrame() already implemented above

bool RenderingService::shouldRender() const {
    return initialized && renderer && gpuEntityManager;
}

void RenderingService::setRenderState(const RenderState& state) {
    renderState_ = state;
    maxRenderableEntities = state.maxRenderableEntities;
}

void RenderingService::setCameraEntity(flecs::entity cameraEntity) {
    cameraEntity_ = cameraEntity;
}

// ECS System Callbacks
void RenderingService::renderPrepareSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable) {
    // Update model matrix
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), transform.position);
    glm::mat4 rotationMatrix = glm::mat4_cast(transform.rotation);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);
    
    renderable.modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    
    // Mark as dirty for GPU sync
    renderable.dirty = true;
}

void RenderingService::cullSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, CullingData& cullingData) {
    // In a full implementation, this would perform frustum culling
    // For now, we'll just mark everything as visible
    cullingData.visible = true;
    cullingData.distance = glm::length(transform.position);
}

void RenderingService::lodSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, LODData& lodData) {
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

void RenderingService::gpuSyncSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable) {
    // This system marks entities that need GPU synchronization
    // The actual sync happens in the service's synchronizeWithGPU method
    if (renderable.dirty) {
        // Mark for GPU sync (this would typically add to a sync queue)
        renderable.dirty = false;
    }
}

// Helper Methods
bool RenderingService::isEntityVisibleInFrustum(const Transform& transform, const Renderable& renderable, 
                                                const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
    // Simple visibility check - in production this would do proper frustum culling
    return true;
}

uint32_t RenderingService::calculateLODLevel(const glm::vec3& entityPosition, const glm::vec3& cameraPosition) {
    float distance = glm::length(entityPosition - cameraPosition);
    
    if (distance < renderState_.lodNearDistance) {
        return 0;
    } else if (distance < renderState_.lodMediumDistance) {
        return 1;
    } else {
        return 2;
    }
}

void RenderingService::updateEntityCullingData(flecs::entity entity, bool visible) {
    if (entity.has<CullingData>()) {
        auto cullingData = entity.get_mut<CullingData>();
        cullingData->visible = visible;
    }
}

void RenderingService::updateEntityLODData(flecs::entity entity, uint32_t lodLevel) {
    if (entity.has<LODData>()) {
        auto lodData = entity.get_mut<LODData>();
        lodData->level = lodLevel;
    }
}

