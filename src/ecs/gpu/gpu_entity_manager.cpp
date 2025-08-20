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

void GPUEntitySoA::addFromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
    // Velocity (initialized to zero, set by compute shader)
    velocities.emplace_back(
        0.0f,                      // velocity.x
        0.0f,                      // velocity.y  
        0.001f,                    // damping factor
        0.0f                       // reserved
    );
    
    // Movement parameters
    movementParams.emplace_back(
        pattern.amplitude,
        pattern.frequency, 
        pattern.phase,
        pattern.timeOffset
    );
    
    // Runtime state
    runtimeStates.emplace_back(
        0.0f,                          // totalTime (updated by compute shader)
        0.0f,                          // reserved 
        stateTimerDist(rng),           // stateTimer (random staggering)
        0.0f                           // initialized flag (starts as 0.0)
    );
    
    // Color
    colors.emplace_back(renderable.color);
    
    // Model matrix  
    modelMatrices.emplace_back(transform.getMatrix());
}

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
    
    entity.runtimeState = glm::vec4(
        0.0f,                          // totalTime (will be updated by compute shader)
        0.0f,                          // reserved 
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
    
    if (!descriptorManager.initialize(context, bufferManager, resourceContext)) {
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
    
    // Convert legacy GPUEntity to SoA format
    stagingEntities.velocities.emplace_back(entity.velocity);
    stagingEntities.movementParams.emplace_back(entity.movementParams);
    stagingEntities.runtimeStates.emplace_back(entity.runtimeState);
    stagingEntities.colors.emplace_back(entity.color);
    stagingEntities.modelMatrices.emplace_back(entity.modelMatrix);
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
            stagingEntities.addFromECS(*transform, *renderable, *movement);
        }
    }
}

void GPUEntityManager::uploadPendingEntities() {
    if (stagingEntities.empty()) return;
    
    std::cout << "GPUEntityManager: WARNING - Uploading entities during runtime! This will overwrite computed positions!" << std::endl;
    
    size_t entityCount = stagingEntities.size();
    
    // Upload each SoA buffer separately
    VkDeviceSize velocityOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize movementParamsOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize runtimeStateOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize colorOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize modelMatrixOffset = activeEntityCount * sizeof(glm::mat4);
    
    VkDeviceSize velocitySize = entityCount * sizeof(glm::vec4);
    VkDeviceSize movementParamsSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize runtimeStateSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize colorSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize modelMatrixSize = entityCount * sizeof(glm::mat4);
    
    // Copy SoA data to GPU buffers
    bufferManager.copyDataToBuffer(bufferManager.getVelocityBuffer(), stagingEntities.velocities.data(), velocitySize, velocityOffset);
    bufferManager.copyDataToBuffer(bufferManager.getMovementParamsBuffer(), stagingEntities.movementParams.data(), movementParamsSize, movementParamsOffset);
    bufferManager.copyDataToBuffer(bufferManager.getRuntimeStateBuffer(), stagingEntities.runtimeStates.data(), runtimeStateSize, runtimeStateOffset);
    bufferManager.copyDataToBuffer(bufferManager.getColorBuffer(), stagingEntities.colors.data(), colorSize, colorOffset);
    bufferManager.copyDataToBuffer(bufferManager.getModelMatrixBuffer(), stagingEntities.modelMatrices.data(), modelMatrixSize, modelMatrixOffset);
    
    // Initialize position buffers with spawn positions
    std::vector<glm::vec4> initialPositions;
    initialPositions.reserve(entityCount);
    
    for (size_t i = 0; i < stagingEntities.modelMatrices.size(); ++i) {
        const auto& modelMatrix = stagingEntities.modelMatrices[i];
        // Extract position from modelMatrix (4th column contains translation)
        glm::vec3 spawnPosition = glm::vec3(modelMatrix[3]);
        initialPositions.emplace_back(spawnPosition, 1.0f);
        
        // Debug first few positions to verify data
        if (i < 5) {
            std::cout << "Entity " << i << " spawn position: (" 
                      << spawnPosition.x << ", " << spawnPosition.y << ", " << spawnPosition.z << ")" << std::endl;
        }
    }
    
    // Initialize ALL position buffers so graphics and physics can read from any of them
    VkDeviceSize positionUploadSize = initialPositions.size() * sizeof(glm::vec4);
    VkDeviceSize positionOffset = activeEntityCount * sizeof(glm::vec4);
    
    bufferManager.copyDataToBuffer(bufferManager.getPositionBuffer(), initialPositions.data(), positionUploadSize, positionOffset);
    bufferManager.copyDataToBuffer(bufferManager.getPositionBufferAlternate(), initialPositions.data(), positionUploadSize, positionOffset);
    bufferManager.copyDataToBuffer(bufferManager.getCurrentPositionBuffer(), initialPositions.data(), positionUploadSize, positionOffset);
    bufferManager.copyDataToBuffer(bufferManager.getTargetPositionBuffer(), initialPositions.data(), positionUploadSize, positionOffset);
    
    activeEntityCount += entityCount;
    stagingEntities.clear();
    
    std::cout << "GPUEntityManager: Uploaded " << entityCount << " entities to GPU-local memory (SoA), total: " << activeEntityCount << std::endl;
}

void GPUEntityManager::clearAllEntities() {
    stagingEntities.clear();
    activeEntityCount = 0;
}

// Core entity logic now clearly visible - descriptor management delegated to EntityDescriptorManager