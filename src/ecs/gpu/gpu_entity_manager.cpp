#include "gpu_entity_manager.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/core/vulkan_sync.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
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
    // Velocity (initialized to zero, set by compute shader) - now supporting 3D
    velocities.emplace_back(
        0.0f,                      // velocity.x
        0.0f,                      // velocity.y
        0.0f,                      // velocity.z (3D support)
        0.001f                     // damping factor moved to w component
    );
    
    // Movement parameters
    movementParams.emplace_back(
        pattern.amplitude,
        pattern.frequency, 
        pattern.phase,
        pattern.timeOffset
    );
    
    // Movement center (3D position) - center point for entity movement
    movementCenters.emplace_back(
        pattern.center.x,              // center.x
        pattern.center.y,              // center.y  
        pattern.center.z,              // center.z (3D support)
        0.0f                           // reserved
    );
    
    // Runtime state
    runtimeStates.emplace_back(
        0.0f,                          // totalTime (updated by compute shader)
        static_cast<float>(static_cast<int>(renderable.entityType)), // entityType (Floor=1, Regular=0)
        stateTimerDist(rng),           // stateTimer (random staggering)
        0.0f                           // initialized flag (starts as 0.0)
    );
    
    // Rotation state (initialized to default values)
    // Start with zero rotation for simplicity
    rotationStates.emplace_back(
        0.0f,                          // current rotation angle in radians
        0.0f,                          // angular velocity (rad/s)
        0.999f,                        // angular damping factor (lighter damping)
        0.0f                           // reserved
    );
    
    // Color
    colors.emplace_back(renderable.color);
    
    // Model matrix  
    modelMatrices.emplace_back(transform.getMatrix());
}


GPUEntityManager::GPUEntityManager() {
}

GPUEntityManager::~GPUEntityManager() {
    cleanup();
}

bool GPUEntityManager::initialize(const VulkanContext& context, VulkanSync* sync, ResourceCoordinator* resourceCoordinator) {
    this->context = &context;
    this->sync = sync;
    this->resourceCoordinator = resourceCoordinator;
    
    // Initialize buffer manager
    if (!bufferManager.initialize(context, resourceCoordinator, MAX_ENTITIES)) {
        std::cerr << "GPUEntityManager: Failed to initialize buffer manager" << std::endl;
        return false;
    }
    
    // Initialize base descriptor manager functionality
    if (!descriptorManager.initialize(context)) {
        std::cerr << "GPUEntityManager: Failed to initialize base descriptor manager" << std::endl;
        return false;
    }
    
    // Initialize entity-specific descriptor manager functionality
    if (!descriptorManager.initializeEntity(bufferManager, resourceCoordinator)) {
        std::cerr << "GPUEntityManager: Failed to initialize entity descriptor manager" << std::endl;
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
            
            // Store mapping from GPU buffer index to ECS entity ID for debugging
            uint32_t gpuIndex = activeEntityCount + stagingEntities.size() - 1;
            if (gpuIndex >= gpuIndexToECSEntity.size()) {
                gpuIndexToECSEntity.resize(gpuIndex + 1);
            }
            gpuIndexToECSEntity[gpuIndex] = entity;
        }
    }
}

void GPUEntityManager::uploadPendingEntities() {
    if (stagingEntities.empty()) return;
    
    bool isRuntimeAddition = (activeEntityCount > 0);
    if (isRuntimeAddition) {
        std::cout << "GPUEntityManager: Adding " << stagingEntities.size() << " new entities at runtime (appending to existing " << activeEntityCount << " entities)" << std::endl;
    } else {
        std::cout << "GPUEntityManager: Initial entity upload (" << stagingEntities.size() << " entities)" << std::endl;
    }
    
    size_t entityCount = stagingEntities.size();
    
    // Upload each SoA buffer separately
    VkDeviceSize velocityOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize movementParamsOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize movementCentersOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize runtimeStateOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize rotationStateOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize colorOffset = activeEntityCount * sizeof(glm::vec4);
    VkDeviceSize modelMatrixOffset = activeEntityCount * sizeof(glm::mat4);
    
    VkDeviceSize velocitySize = entityCount * sizeof(glm::vec4);
    VkDeviceSize movementParamsSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize movementCentersSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize runtimeStateSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize rotationStateSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize colorSize = entityCount * sizeof(glm::vec4);
    VkDeviceSize modelMatrixSize = entityCount * sizeof(glm::mat4);
    
    // Copy SoA data to GPU buffers using new typed upload methods
    bufferManager.uploadVelocityData(stagingEntities.velocities.data(), velocitySize, velocityOffset);
    bufferManager.uploadMovementParamsData(stagingEntities.movementParams.data(), movementParamsSize, movementParamsOffset);
    bufferManager.uploadMovementCentersData(stagingEntities.movementCenters.data(), movementCentersSize, movementCentersOffset);
    bufferManager.uploadRuntimeStateData(stagingEntities.runtimeStates.data(), runtimeStateSize, runtimeStateOffset);
    bufferManager.uploadRotationStateData(stagingEntities.rotationStates.data(), rotationStateSize, rotationStateOffset);
    bufferManager.uploadColorData(stagingEntities.colors.data(), colorSize, colorOffset);
    
    // Model matrices: Always upload for new entities (they need initial positions)
    // Note: This is safe because we're appending at the correct offset, not overwriting existing entities
    bufferManager.uploadModelMatrixData(stagingEntities.modelMatrices.data(), modelMatrixSize, modelMatrixOffset);
    
    // MVP APPROACH: Initial positions are now stored directly in model matrices
    // Physics shader will read/write positions from/to model matrix column 3
    // Debug model matrix positions to verify proper initialization
    for (size_t i = 0; i < stagingEntities.modelMatrices.size() && i < 5; ++i) {
        const auto& modelMatrix = stagingEntities.modelMatrices[i];
        glm::vec3 spawnPosition = glm::vec3(modelMatrix[3]);
        std::cout << "Entity " << i << " model matrix position: (" 
                  << spawnPosition.x << ", " << spawnPosition.y << ", " << spawnPosition.z << ")" << std::endl;
    }
    
    activeEntityCount += entityCount;
    stagingEntities.clear();
    
    std::cout << "GPUEntityManager: Uploaded " << entityCount << " entities to GPU-local memory (SoA), total: " << activeEntityCount << std::endl;
}

void GPUEntityManager::clearAllEntities() {
    stagingEntities.clear();
    activeEntityCount = 0;
}

// Core entity logic now clearly visible - descriptor management delegated to EntityDescriptorManager

flecs::entity GPUEntityManager::getECSEntityFromGPUIndex(uint32_t gpuIndex) const {
    if (gpuIndex < gpuIndexToECSEntity.size()) {
        return gpuIndexToECSEntity[gpuIndex];
    }
    return flecs::entity{}; // Invalid entity
}