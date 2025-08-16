#include "gpu_entity_manager.h"
#include "../vulkan/vulkan_context.h"
#include "../vulkan/vulkan_sync.h"
#include "../vulkan/vulkan_function_loader.h"
#include "../vulkan/vulkan_utils.h"
#include "../vulkan/resource_context.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cassert>

GPUEntityManager::GPUEntityManager() {
    // Buffers will be initialized in initialize()
}

GPUEntityManager::~GPUEntityManager() {
    cleanup();
}

bool GPUEntityManager::initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext, VkDescriptorSetLayout computeDescriptorSetLayout) {
    this->context = &context;
    this->sync = sync;
    this->resourceContext = resourceContext;
    
    if (!createEntityBuffers()) {
        std::cerr << "Failed to create entity buffers!" << std::endl;
        return false;
    }
    
    if (!createComputeDescriptorPool()) {
        std::cerr << "Failed to create compute descriptor pool!" << std::endl;
        return false;
    }
    
    // Use provided layout from VulkanPipeline
    this->computeDescriptorSetLayout = computeDescriptorSetLayout;
    
    if (!createComputeDescriptorSets()) {
        std::cerr << "Failed to create compute descriptor sets!" << std::endl;
        return false;
    }
    
    return true;
}

void GPUEntityManager::cleanup() {
    if (!context) return;
    
    // Clean up GPU buffers using GPUBufferRing
    if (entityBuffer) {
        entityBuffer->cleanup();
        entityBuffer.reset();
    }
    
    if (positionBuffer) {
        positionBuffer->cleanup();
        positionBuffer.reset();
    }
    
    if (currentPositionBuffer) {
        currentPositionBuffer->cleanup();
        currentPositionBuffer.reset();
    }
    
    if (targetPositionBuffer) {
        targetPositionBuffer->cleanup();
        targetPositionBuffer.reset();
    }
    
    
    // Clean up descriptor resources
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        if (resourceContext) {
            resourceContext->destroyDescriptorPool(computeDescriptorPool);
        }
        computeDescriptorPool = VK_NULL_HANDLE;
    }
    
    // Layout is owned by VulkanPipeline, don't destroy it here
    computeDescriptorSetLayout = VK_NULL_HANDLE;
}

void GPUEntityManager::addEntity(const GPUEntity& entity) {
    if (activeEntityCount >= MAX_ENTITIES) {
        std::cerr << "GPU entity buffer full, cannot add more entities!" << std::endl;
        return;
    }
    
    // Use GPUBufferRing's staging functionality
    if (entityBuffer && entityBuffer->addData(&entity, sizeof(GPUEntity), alignof(GPUEntity))) {
        activeEntityCount++;
    } else {
        std::cerr << "Failed to add entity to buffer!" << std::endl;
    }
}

void GPUEntityManager::addEntitiesFromECS(const std::vector<Entity>& entities) {
    for (const auto& entity : entities) {
        auto* transform = entity.get<Transform>();
        auto* renderable = entity.get<Renderable>();
        auto* pattern = entity.get<MovementPattern>();
        
        if (transform && renderable && pattern) {
            addEntity(GPUEntity::fromECS(*transform, *renderable, *pattern));
        }
    }
}

void GPUEntityManager::flushStagingBuffer() {
    if (!entityBuffer || !entityBuffer->hasPendingData()) {
        return;
    }
    
    // Calculate where to write new entities in the buffer
    // New entities go after the ones already flushed to GPU
    VkDeviceSize dstOffset = lastFlushedCount * sizeof(GPUEntity);
    
    // Flush entity buffer to GPU at the calculated offset
    entityBuffer->flushToGPU(dstOffset);
    
    // Update the count of entities flushed to GPU
    lastFlushedCount = activeEntityCount;
    
#ifdef _DEBUG
    uint32_t newEntities = activeEntityCount - (dstOffset / sizeof(GPUEntity));
    std::cout << "Flushed " << newEntities << " entities to GPU. Total: " << activeEntityCount << std::endl;
#endif
}


void GPUEntityManager::clearAllEntities() {
    activeEntityCount = 0;
    lastFlushedCount = 0;
    if (entityBuffer) {
        entityBuffer->resetStaging();
    }
}

// Buffer getter implementations
VkBuffer GPUEntityManager::getCurrentEntityBuffer() const {
    return entityBuffer ? entityBuffer->getBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUEntityManager::getCurrentPositionBuffer() const {
    return positionBuffer ? positionBuffer->getBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUEntityManager::getCurrentPositionStorageBuffer() const {
    return currentPositionBuffer ? currentPositionBuffer->getBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUEntityManager::getTargetPositionStorageBuffer() const {
    return targetPositionBuffer ? targetPositionBuffer->getBuffer() : VK_NULL_HANDLE;
}

bool GPUEntityManager::hasPendingUploads() const {
    return entityBuffer && entityBuffer->hasPendingData();
}





bool GPUEntityManager::createEntityBuffers() {
    // Create entity buffer using GPUBufferRing (input for compute shader)
    entityBuffer = std::make_unique<GPUBufferRing>();
    if (!entityBuffer->initialize(
            resourceContext,
            ENTITY_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        std::cerr << "Failed to create entity buffer!" << std::endl;
        return false;
    }
    
    // Create position buffer using GPUBufferRing (output from compute shader)
    positionBuffer = std::make_unique<GPUBufferRing>();
    if (!positionBuffer->initialize(
            resourceContext,
            POSITION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        std::cerr << "Failed to create position buffer!" << std::endl;
        return false;
    }
    
    // Create current position buffer for interpolation
    currentPositionBuffer = std::make_unique<GPUBufferRing>();
    if (!currentPositionBuffer->initialize(
            resourceContext,
            POSITION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        std::cerr << "Failed to create current position buffer!" << std::endl;
        return false;
    }
    
    // Create target position buffer for interpolation
    targetPositionBuffer = std::make_unique<GPUBufferRing>();
    if (!targetPositionBuffer->initialize(
            resourceContext,
            POSITION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        std::cerr << "Failed to create target position buffer!" << std::endl;
        return false;
    }
    
    return true;
}

bool GPUEntityManager::createComputeDescriptorPool() {
    // Use ResourceContext for descriptor pool creation
    ResourceContext::DescriptorPoolConfig config;
    config.maxSets = 1; // Single unified descriptor set
    config.uniformBuffers = 0;
    config.storageBuffers = 4; // Single set * 4 buffers
    config.sampledImages = 0;
    config.allowFreeDescriptorSets = true;
    
    computeDescriptorPool = resourceContext->createDescriptorPool(config);
    
    return computeDescriptorPool != VK_NULL_HANDLE;
}


bool GPUEntityManager::createComputeDescriptorSets() {
    // Allocate single descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computeDescriptorSetLayout;
    
    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        return false;
    }
    
    // Update descriptor set with proper input and output buffers
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = entityBuffer->getBuffer();
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = ENTITY_BUFFER_SIZE;
    
    VkDescriptorBufferInfo positionBufferInfo{};
    positionBufferInfo.buffer = positionBuffer->getBuffer();
    positionBufferInfo.offset = 0;
    positionBufferInfo.range = POSITION_BUFFER_SIZE;
    
    VkDescriptorBufferInfo currentPositionBufferInfo{};
    currentPositionBufferInfo.buffer = currentPositionBuffer->getBuffer();
    currentPositionBufferInfo.offset = 0;
    currentPositionBufferInfo.range = POSITION_BUFFER_SIZE;
    
    VkDescriptorBufferInfo targetPositionBufferInfo{};
    targetPositionBufferInfo.buffer = targetPositionBuffer->getBuffer();
    targetPositionBufferInfo.offset = 0;
    targetPositionBufferInfo.range = POSITION_BUFFER_SIZE;
    
    // Input buffer (binding 0), output buffer (binding 1), current positions (binding 2), and target positions (binding 3)
    std::vector<VkDescriptorBufferInfo> bufferInfos = {entityBufferInfo, positionBufferInfo, currentPositionBufferInfo, targetPositionBufferInfo};
    VulkanUtils::writeDescriptorSets(context->getDevice(), context->getLoader(), computeDescriptorSet, bufferInfos);
    
    return true;
}

bool GPUEntityManager::recreateComputeDescriptorResources(VkDescriptorSetLayout newLayout) {
    // Update layout reference
    computeDescriptorSetLayout = newLayout;
    
    // Reset the descriptor pool to free all allocated descriptor sets
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        VkResult result = context->getLoader().vkResetDescriptorPool(context->getDevice(), computeDescriptorPool, 0);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to reset compute descriptor pool: " << result << std::endl;
            return false;
        }
        computeDescriptorSet = VK_NULL_HANDLE; // Pool reset invalidates descriptor sets
    }
    
    // Recreate descriptor sets with the new layout
    return createComputeDescriptorSets();
}

