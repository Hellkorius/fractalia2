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
    // No longer need to pre-allocate pendingEntities vector
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
    
    // Use provided layout if available, otherwise create our own
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        this->computeDescriptorSetLayout = computeDescriptorSetLayout;
        this->ownsDescriptorSetLayout = false;
    } else {
        if (!createComputeDescriptorSetLayout()) {
            std::cerr << "Failed to create compute descriptor set layout!" << std::endl;
            return false;
        }
        this->ownsDescriptorSetLayout = true;
    }
    
    if (!createComputeDescriptorSets()) {
        std::cerr << "Failed to create compute descriptor sets!" << std::endl;
        return false;
    }
    
    return true;
}

void GPUEntityManager::cleanup() {
    if (!context) return;
    
    // Clean up entity buffer using ResourceContext
    if (entityStorageHandle && resourceContext) {
        resourceContext->destroyResource(*entityStorageHandle);
        entityStorageHandle.reset();
    }
    
    // Clean up position buffers
    if (positionStorageHandle && resourceContext) {
        resourceContext->destroyResource(*positionStorageHandle);
        positionStorageHandle.reset();
    }
    
    
    if (currentPositionStorageHandle && resourceContext) {
        resourceContext->destroyResource(*currentPositionStorageHandle);
        currentPositionStorageHandle.reset();
    }
    
    if (targetPositionStorageHandle && resourceContext) {
        resourceContext->destroyResource(*targetPositionStorageHandle);
        targetPositionStorageHandle.reset();
    }
    
    
    // Clean up descriptor resources
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        if (resourceContext) {
            resourceContext->destroyDescriptorPool(computeDescriptorPool);
        }
        computeDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (computeDescriptorSetLayout != VK_NULL_HANDLE && ownsDescriptorSetLayout) {
        context->getLoader().vkDestroyDescriptorSetLayout(context->getDevice(), computeDescriptorSetLayout, nullptr);
        computeDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void GPUEntityManager::addEntity(const GPUEntity& entity) {
    if (activeEntityCount >= MAX_ENTITIES) {
        std::cerr << "GPU entity buffer full, cannot add more entities!" << std::endl;
        return;
    }
    
    // Write directly to staging buffer
    auto stagingRegion = resourceContext->getStagingBuffer().allocate(sizeof(GPUEntity), alignof(GPUEntity));
    if (!stagingRegion.mappedData) {
        // Reset and try again
        resourceContext->getStagingBuffer().reset();
        stagingBytesWritten = 0;
        stagingStartOffset = 0;
        stagingRegion = resourceContext->getStagingBuffer().allocate(sizeof(GPUEntity), alignof(GPUEntity));
    }
    
    if (stagingRegion.mappedData) {
        memcpy(stagingRegion.mappedData, &entity, sizeof(GPUEntity));
        if (stagingBytesWritten == 0) {
            // Track where our batch starts
            stagingStartOffset = stagingRegion.offset;
        }
        stagingBytesWritten += sizeof(GPUEntity);
        activeEntityCount++;
        needsUpload = true;
    } else {
        std::cerr << "Failed to allocate staging buffer for entity!" << std::endl;
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
    if (!needsUpload || stagingBytesWritten == 0) {
        return;
    }
    
    // Calculate how many entities were written to staging buffer
    uint32_t entityCount = static_cast<uint32_t>(stagingBytesWritten / sizeof(GPUEntity));
    if (entityCount == 0) {
        return;
    }
    
    // Calculate offset in destination buffer
    VkDeviceSize dstOffset = (activeEntityCount - entityCount) * sizeof(GPUEntity);
    
    // Get staging buffer handle
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    ResourceHandle stagingHandle;
    stagingHandle.buffer = stagingBuffer.getBuffer();
    
    // Copy from staging buffer to device-local entity buffer
    // This performs a single vkCmdCopyBuffer command
    resourceContext->copyBufferToBuffer(
        stagingHandle,
        *entityStorageHandle,
        stagingBytesWritten,
        stagingStartOffset, // Use the actual offset where our data starts
        dstOffset
    );
    
    // Reset staging state
    stagingBuffer.reset();
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
    
#ifdef _DEBUG
    std::cout << "Flushed " << entityCount << " entities from staging buffer. Total: " << activeEntityCount << std::endl;
#endif
}


void GPUEntityManager::clearAllEntities() {
    activeEntityCount = 0;
    resourceContext->getStagingBuffer().reset();
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}





bool GPUEntityManager::createEntityBuffers() {
    // Create entity buffer using ResourceContext (input for compute shader)
    // Use DEVICE_LOCAL memory with TRANSFER_DST for optimal performance
    entityStorageHandle = std::make_unique<ResourceHandle>(
        resourceContext->createBuffer(
            ENTITY_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    );
    
    if (!entityStorageHandle->isValid()) {
        std::cerr << "Failed to create entity buffer!" << std::endl;
        entityStorageHandle.reset();
        return false;
    }
    
    // Create position buffer using ResourceContext (output from compute shader)
    positionStorageHandle = std::make_unique<ResourceHandle>(
        resourceContext->createMappedBuffer(
            POSITION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    );
    
    if (!positionStorageHandle->isValid()) {
        std::cerr << "Failed to create position buffer!" << std::endl;
        positionStorageHandle.reset();
        entityStorageHandle.reset();
        return false;
    }
    
    
    // Create current position buffer for interpolation
    currentPositionStorageHandle = std::make_unique<ResourceHandle>(
        resourceContext->createMappedBuffer(
            POSITION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    );
    
    if (!currentPositionStorageHandle->isValid()) {
        std::cerr << "Failed to create current position buffer!" << std::endl;
        currentPositionStorageHandle.reset();
        positionStorageHandle.reset();
        entityStorageHandle.reset();
        return false;
    }
    
    // Create target position buffer for interpolation
    targetPositionStorageHandle = std::make_unique<ResourceHandle>(
        resourceContext->createMappedBuffer(
            POSITION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    );
    
    if (!targetPositionStorageHandle->isValid()) {
        std::cerr << "Failed to create target position buffer!" << std::endl;
        targetPositionStorageHandle.reset();
        currentPositionStorageHandle.reset();
        positionStorageHandle.reset();
        entityStorageHandle.reset();
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

bool GPUEntityManager::createComputeDescriptorSetLayout() {
    // Four storage buffers: entities, positions, currentPositions, targetPositions
    VkDescriptorSetLayoutBinding bindings[4] = {};
    
    // Input buffer binding (entities)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Output buffer binding (positions)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Current positions buffer binding
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Target positions buffer binding
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;
    
    return context->getLoader().vkCreateDescriptorSetLayout(context->getDevice(), &layoutInfo, nullptr, &computeDescriptorSetLayout) == VK_SUCCESS;
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
    entityBufferInfo.buffer = entityStorageHandle->buffer;
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = ENTITY_BUFFER_SIZE;
    
    VkDescriptorBufferInfo positionBufferInfo{};
    positionBufferInfo.buffer = positionStorageHandle->buffer;
    positionBufferInfo.offset = 0;
    positionBufferInfo.range = POSITION_BUFFER_SIZE;
    
    VkDescriptorBufferInfo currentPositionBufferInfo{};
    currentPositionBufferInfo.buffer = currentPositionStorageHandle->buffer;
    currentPositionBufferInfo.offset = 0;
    currentPositionBufferInfo.range = POSITION_BUFFER_SIZE;
    
    VkDescriptorBufferInfo targetPositionBufferInfo{};
    targetPositionBufferInfo.buffer = targetPositionStorageHandle->buffer;
    targetPositionBufferInfo.offset = 0;
    targetPositionBufferInfo.range = POSITION_BUFFER_SIZE;
    
    // Input buffer (binding 0), output buffer (binding 1), current positions (binding 2), and target positions (binding 3)
    std::vector<VkDescriptorBufferInfo> bufferInfos = {entityBufferInfo, positionBufferInfo, currentPositionBufferInfo, targetPositionBufferInfo};
    VulkanUtils::writeDescriptorSets(context->getDevice(), context->getLoader(), computeDescriptorSet, bufferInfos);
    
    return true;
}

bool GPUEntityManager::recreateComputeDescriptorResources() {
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

