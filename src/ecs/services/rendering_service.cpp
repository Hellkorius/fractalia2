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
    
    // Setup ECS integration
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
    
    // Collect entities that need GPU upload using SoA approach
    std::vector<flecs::entity> entitiesToUpload;
    
    // Query for entities that need GPU upload
    world->query<Transform, Renderable>().each([&entitiesToUpload](flecs::entity entity, 
                                                                   Transform& transform, 
                                                                   Renderable& renderable) {
        // Check if entity has GPU upload pending tag
        if (!entity.has<GPUUploadPending>()) {
            return;
        }
        
        // Ensure entity has MovementPattern component (add default if missing)
        if (!entity.has<MovementPattern>()) {
            entity.add<MovementPattern>();
        }
        
        entitiesToUpload.push_back(entity);
    });
    
    // Batch upload using SoA approach for better performance
    if (!entitiesToUpload.empty()) {
        gpuEntityManager->addEntitiesFromECS(entitiesToUpload);
        
        // Mark all uploaded entities as complete
        for (auto& entity : entitiesToUpload) {
            entity.remove<GPUUploadPending>();
            entity.add<GPUUploadComplete>();
        }
    }
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
    if (!initialized || !gpuEntityManager) {
        return;
    }
    
    // GPU-DRIVEN RENDERING OPTIMIZATION:
    // Since movement is GPU-driven, ECS Transform components are stale (never updated).
    // Skip expensive per-entity queries. Let GPU handle all positioning and culling.
    
    uint32_t entityCount = gpuEntityManager->getEntityCount();
    if (entityCount > 0) {
        // Create single batch render entry for all GPU entities
        RenderQueueEntry batchEntry{};
        batchEntry.entity = flecs::entity::null(); // Batch render, no specific entity
        batchEntry.priority = RenderPriority::NORMAL;
        batchEntry.distanceToCamera = 0.0f; // GPU calculates distances
        batchEntry.lodLevel = 0; // GPU handles LOD
        batchEntry.visible = true;
        
        renderQueue.push_back(batchEntry);
        
        // Note: entityCount stored implicitly via GPU entity manager, not in queue entry
    }
    
    // NOTE: If you need per-entity ECS rendering for debugging, re-enable this:
    /*
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
    */
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
// ECS Integration Methods
// ============================================================================

void RenderingService::setupRenderingPhases() {
    // GPU-DRIVEN PIPELINE: Rendering phases removed for performance
    // All entity processing handled by GPU compute shaders, no CPU-side phases needed
}

void RenderingService::registerRenderingSystems() {
    // GPU-DRIVEN PIPELINE: CPU-side ECS systems removed for performance
    // All entity processing (transforms, culling, LOD) handled by GPU compute shaders
    // This eliminates 320k function calls per frame (4 systems × 80k entities)
}

void RenderingService::cleanupSystems() {
    // GPU-DRIVEN PIPELINE: No CPU-side ECS systems to clean up
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
// GPU-DRIVEN PIPELINE: System callbacks removed
// All entity processing (transforms, culling, LOD, sync) handled by GPU compute shaders
// This eliminates 19.2 million function calls per second (320k calls/frame × 60 FPS)

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

