#include "gpu_entity_manager.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/core/vulkan_sync.h"
#include "../../vulkan/resources/resource_context.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include "../../vulkan/core/vulkan_utils.h"
#include <iostream>
#include <cstring>
#include <random>
#include <array>

// Static RNG for performance - initialized once per thread
thread_local std::mt19937 rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> stateTimerDist{0.0f, 600.0f};

GPUEntity GPUEntity::fromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
    GPUEntity entity{};
    
    // Initialize velocity to zero (will be set by movement compute shader)
    entity.velocity = glm::vec4(
        0.0f,                      // velocity.x
        0.0f,                      // velocity.y  
        0.001f,                    // damping factor (very small amount of drag)
        0.0f                       // reserved
    );
    
    entity.movementParams = glm::vec4(
        pattern.amplitude,
        pattern.frequency, 
        pattern.phase,
        pattern.timeOffset
    );
    
    entity.color = renderable.color;
    
    entity.modelMatrix = transform.getMatrix();
    
    // Set the center position for movement (use model matrix translation as center)
    glm::vec3 centerPos = glm::vec3(entity.modelMatrix[3]); // Extract translation from model matrix
    entity.runtimeState = glm::vec4(
        centerPos.x,                   // center.x 
        centerPos.y,                   // center.y
        stateTimerDist(rng),           // stateTimer (random staggering) - optimized RNG
        0.0f                           // initialized flag (must start as 0.0 for GPU initialization)
    );
    
    return entity;
}

GPUEntityManager::GPUEntityManager() {
}

GPUEntityManager::~GPUEntityManager() {
    cleanup();
}

bool GPUEntityManager::initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext) {
    this->context = &context;
    this->sync = sync;
    this->resourceContext = resourceContext;
    
    // Initialize buffer manager
    if (!bufferManager.initialize(context, resourceContext, MAX_ENTITIES)) {
        std::cerr << "GPUEntityManager: Failed to initialize buffer manager" << std::endl;
        return false;
    }
    
    if (!descriptorManager.initialize(context, bufferManager)) {
        std::cerr << "GPUEntityManager: Failed to initialize descriptor manager" << std::endl;
        return false;
    }
    
    std::cout << "GPUEntityManager: Initialized successfully with descriptor manager" << std::endl;
    return true;
}

void GPUEntityManager::cleanup() {
    if (!context) return;
    
    // Cleanup descriptor manager first
    descriptorManager.cleanup();
    
    // Cleanup buffer manager
    bufferManager.cleanup();
}

void GPUEntityManager::addEntity(const GPUEntity& entity) {
    if (activeEntityCount + stagingEntities.size() >= bufferManager.getMaxEntities()) {
        std::cerr << "GPUEntityManager: Cannot add entity, would exceed max capacity" << std::endl;
        return;
    }
    
    stagingEntities.push_back(entity);
}

void GPUEntityManager::addEntitiesFromECS(const std::vector<flecs::entity>& entities) {
    for (const auto& entity : entities) {
        if (activeEntityCount + stagingEntities.size() >= MAX_ENTITIES) {
            std::cerr << "GPUEntityManager: Reached max capacity, stopping entity addition" << std::endl;
            break;
        }
        
        // Get components from entity using .get<>()
        const Transform* transform = entity.get<Transform>();
        const Renderable* renderable = entity.get<Renderable>();
        const MovementPattern* movement = entity.get<MovementPattern>();
        
        if (transform && renderable && movement) {
            GPUEntity gpuEntity = GPUEntity::fromECS(*transform, *renderable, *movement);
            stagingEntities.push_back(gpuEntity);
        }
    }
}

void GPUEntityManager::uploadPendingEntities() {
    if (stagingEntities.empty()) return;
    
    // Copy staging entities to GPU buffer using ResourceContext staging infrastructure
    VkDeviceSize uploadSize = stagingEntities.size() * sizeof(GPUEntity);
    VkDeviceSize offset = activeEntityCount * sizeof(GPUEntity);
    
    // Use buffer manager to copy data with proper offset for appending
    bufferManager.copyDataToBuffer(bufferManager.getEntityBuffer(), stagingEntities.data(), uploadSize, offset);
    
    // PERFECT PING-PONG FIX: Initialize both ping-pong buffers with spawn positions
    // This ensures graphics always reads correct data regardless of frame timing
    std::vector<glm::vec4> initialPositions;
    initialPositions.reserve(stagingEntities.size());
    
    for (const auto& entity : stagingEntities) {
        // Extract position from modelMatrix (4th column contains translation)
        glm::vec3 spawnPosition = glm::vec3(entity.modelMatrix[3]);
        initialPositions.emplace_back(spawnPosition, 1.0f);
    }
    
    // Initialize ALL position buffers so graphics and physics can read from any of them
    VkDeviceSize positionUploadSize = initialPositions.size() * sizeof(glm::vec4);
    VkDeviceSize positionOffset = activeEntityCount * sizeof(glm::vec4);
    
    bufferManager.copyDataToBuffer(bufferManager.getPositionBuffer(), initialPositions.data(), positionUploadSize, positionOffset);
    bufferManager.copyDataToBuffer(bufferManager.getPositionBufferAlternate(), initialPositions.data(), positionUploadSize, positionOffset);
    bufferManager.copyDataToBuffer(bufferManager.getCurrentPositionBuffer(), initialPositions.data(), positionUploadSize, positionOffset);
    bufferManager.copyDataToBuffer(bufferManager.getTargetPositionBuffer(), initialPositions.data(), positionUploadSize, positionOffset);
    
    
    activeEntityCount += stagingEntities.size();
    stagingEntities.clear();
    
    std::cout << "GPUEntityManager: Uploaded " << (uploadSize / sizeof(GPUEntity)) << " entities to GPU-local memory, total: " << activeEntityCount << std::endl;
}

void GPUEntityManager::clearAllEntities() {
    stagingEntities.clear();
    activeEntityCount = 0;
}

// Core entity logic now clearly visible - descriptor management delegated to EntityDescriptorManager