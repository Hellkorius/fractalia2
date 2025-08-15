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
    pendingEntities.reserve(10000); // Pre-allocate for batch uploads
}

GPUEntityManager::~GPUEntityManager() {
    cleanup();
}

bool GPUEntityManager::initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext) {
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
    
    if (!createComputeDescriptorSetLayout()) {
        std::cerr << "Failed to create compute descriptor set layout!" << std::endl;
        return false;
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
    
    entityStorage = VK_NULL_HANDLE;
    
    // Clean up descriptor resources
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        if (resourceContext) {
            resourceContext->destroyDescriptorPool(computeDescriptorPool);
        }
        computeDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDescriptorSetLayout(context->getDevice(), computeDescriptorSetLayout, nullptr);
        computeDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void GPUEntityManager::addEntity(const GPUEntity& entity) {
    pendingEntities.push_back(entity);
}

void GPUEntityManager::addEntitiesFromECS(const std::vector<Entity>& entities) {
    for (const auto& entity : entities) {
        auto* transform = entity.get<Transform>();
        auto* renderable = entity.get<Renderable>();
        auto* pattern = entity.get<MovementPattern>();
        
        if (transform && renderable && pattern) {
            GPUEntity gpuEntity = GPUEntity::fromECS(*transform, *renderable, *pattern);
            addEntity(gpuEntity);
        }
    }
}

void GPUEntityManager::uploadPendingEntities() {
    if (pendingEntities.empty()) {
        return;
    }
    
    uint32_t requestedCount = static_cast<uint32_t>(pendingEntities.size());
    uint32_t availableSpace = (activeEntityCount < MAX_ENTITIES) ? (MAX_ENTITIES - activeEntityCount) : 0;
    
    uint32_t newEntityCount = std::min(requestedCount, availableSpace);
    
#ifdef _DEBUG
    assert(newEntityCount > 0 && "GPU entity buffer must have available space");
    assert(activeEntityCount + newEntityCount <= MAX_ENTITIES && "Must not exceed buffer capacity");
#endif
    
    if (newEntityCount == 0) {
        return;
    }
    
    // Upload new entities to single buffer
    GPUEntity* targetBufferPtr = static_cast<GPUEntity*>(getMappedData());
#ifdef _DEBUG
    assert(targetBufferPtr && "GPU target buffer must be mapped");
#endif
    
    std::memcpy(
        targetBufferPtr + activeEntityCount,
        pendingEntities.data(),
        newEntityCount * sizeof(GPUEntity)
    );
    
    activeEntityCount += newEntityCount;
    
    // Clear uploaded entities
    if (newEntityCount == pendingEntities.size()) {
        pendingEntities.clear();
    } else {
        pendingEntities.erase(pendingEntities.begin(), pendingEntities.begin() + newEntityCount);
    }
    
#ifdef _DEBUG
    std::cout << "Uploaded " << newEntityCount << " entities to GPU. Total: " << activeEntityCount << std::endl;
#endif
}


void GPUEntityManager::clearAllEntities() {
    activeEntityCount = 0;
    pendingEntities.clear();
}

void* GPUEntityManager::getMappedData() const {
    return entityStorageHandle ? entityStorageHandle->mappedData : nullptr;
}

void GPUEntityManager::updateAllMovementTypes(int newMovementType, bool angelMode) {
    if (activeEntityCount == 0) {
        return;
    }
    
    // Force GPU synchronization to ensure no buffers are being actively processed
    // This gives us absolute control over buffer contents
    if (context) {
        context->getLoader().vkDeviceWaitIdle(context->getDevice());
    }
    
    // Update single buffer entities
    GPUEntity* bufferPtr = static_cast<GPUEntity*>(getMappedData());
    
    if (bufferPtr) {
        for (uint32_t i = 0; i < activeEntityCount; i++) {
            // Update the movementType field (w component of movementParams1)
            bufferPtr[i].movementParams1.w = static_cast<float>(newMovementType);
            
            if (angelMode) {
                // Angel Mode: Trigger biblical transition via origin
                bufferPtr[i].runtimeState.z = 0.0f; // Reset state timer
                bufferPtr[i].runtimeState.w = 2.0f; // STATE_ANGEL_TRANSITION
            } else {
                // Organic Mode: Trigger direct transition to target position
                bufferPtr[i].runtimeState.z = 0.0f; // Reset state timer
                bufferPtr[i].runtimeState.w = 3.0f; // STATE_ORGANIC_TRANSITION
            }
            
            // Keep initialized flag so center is preserved during transition
            // (Don't reset runtimeState.y - keep it as initialized)
        }
    }
    
    if (angelMode) {
        std::cout << "Started ANGEL MODE transition for " << activeEntityCount << " GPU entities to movement type " << newMovementType 
                  << " (biblical 2-second transition via origin)" << std::endl;
    } else {
        std::cout << "Started organic transition for " << activeEntityCount << " GPU entities to movement type " << newMovementType 
                  << " (direct movement to target positions)" << std::endl;
    }
}



bool GPUEntityManager::createEntityBuffers() {
    // Create entity buffer using ResourceContext
    entityStorageHandle = std::make_unique<ResourceHandle>(
        resourceContext->createMappedBuffer(
            ENTITY_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        )
    );
    
    if (!entityStorageHandle->isValid()) {
        std::cerr << "Failed to create entity buffer!" << std::endl;
        entityStorageHandle.reset();
        return false;
    }
    
    // Maintain compatibility with existing API
    entityStorage = entityStorageHandle->buffer;
    
    return true;
}

bool GPUEntityManager::createComputeDescriptorPool() {
    // Use ResourceContext for descriptor pool creation
    ResourceContext::DescriptorPoolConfig config;
    config.maxSets = 1;
    config.uniformBuffers = 0;
    config.storageBuffers = 2; // 1 set * 2 buffers (input/output)
    config.sampledImages = 0;
    config.allowFreeDescriptorSets = true;
    
    computeDescriptorPool = resourceContext->createDescriptorPool(config);
    
    return computeDescriptorPool != VK_NULL_HANDLE;
}

bool GPUEntityManager::createComputeDescriptorSetLayout() {
    // Two storage buffers: input and output
    VkDescriptorSetLayoutBinding bindings[2] = {};
    
    // Input buffer binding
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Output buffer binding  
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
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
    
    // Update descriptor set - both input and output point to same buffer
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = entityStorage;
    bufferInfo.offset = 0;
    bufferInfo.range = ENTITY_BUFFER_SIZE;
    
    // Use helper for both input (binding 0) and output (binding 1) buffers
    std::vector<VkDescriptorBufferInfo> bufferInfos = {bufferInfo, bufferInfo};
    VulkanUtils::writeDescriptorSets(context->getDevice(), context->getLoader(), computeDescriptorSet, bufferInfos);
    
    return true;
}

