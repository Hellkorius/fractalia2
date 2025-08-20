#include "gpu_entity_manager.h"
#include "../vulkan/core/vulkan_context.h"
#include "../vulkan/core/vulkan_sync.h"
#include "../vulkan/resources/resource_context.h"
#include "../vulkan/core/vulkan_function_loader.h"
#include "../vulkan/core/vulkan_utils.h"
#include <iostream>
#include <cstring>
#include <random>
#include <array>

GPUEntity GPUEntity::fromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
    GPUEntity entity{};
    
    entity.movementParams0 = glm::vec4(
        pattern.amplitude,
        pattern.frequency, 
        pattern.phase,
        pattern.timeOffset
    );
    
    entity.movementParams1 = glm::vec4(
        pattern.center.x,
        pattern.center.y, 
        pattern.center.z,
        static_cast<float>(pattern.type)
    );
    
    entity.color = renderable.color;
    
    entity.modelMatrix = transform.getMatrix();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 600.0f);
    
    entity.runtimeState = glm::vec4(
        0.0f,           // totalTime (will be updated by compute shader)
        0.0f,           // initialized flag (must start as 0.0 for GPU initialization)
        dis(gen),       // stateTimer (random staggering)
        0.0f            // entityState (reserved)
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
    
    activeEntityCount += stagingEntities.size();
    stagingEntities.clear();
    
    std::cout << "GPUEntityManager: Uploaded " << (uploadSize / sizeof(GPUEntity)) << " entities to GPU-local memory, total: " << activeEntityCount << std::endl;
}

void GPUEntityManager::clearAllEntities() {
    stagingEntities.clear();
    activeEntityCount = 0;
}

// Core entity logic now clearly visible - descriptor management delegated to EntityDescriptorManager