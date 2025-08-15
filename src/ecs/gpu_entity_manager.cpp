#include "gpu_entity_manager.h"
#include "../vulkan/vulkan_context.h"
#include "../vulkan/vulkan_sync.h"
#include "../vulkan/vulkan_function_loader.h"
#include "../vulkan/vulkan_utils.h"
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

bool GPUEntityManager::initialize(VulkanContext* context, VulkanSync* sync, VulkanFunctionLoader* loader) {
    this->context = context;
    this->sync = sync;
    this->loader = loader;
    
    // Temporary: If no loader provided, we can't proceed
    if (!loader) {
        std::cerr << "GPUEntityManager requires VulkanFunctionLoader" << std::endl;
        return false;
    }
    
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
    if (!loader || !context) return;
    
    // Clean up entity buffer
    if (entityBufferMapped != nullptr) {
        loader->vkUnmapMemory(context->getDevice(), entityMemory);
        entityBufferMapped = nullptr;
    }
    
    if (entityStorage != VK_NULL_HANDLE) {
        loader->vkDestroyBuffer(context->getDevice(), entityStorage, nullptr);
        entityStorage = VK_NULL_HANDLE;
    }
    
    if (entityMemory != VK_NULL_HANDLE) {
        loader->vkFreeMemory(context->getDevice(), entityMemory, nullptr);
        entityMemory = VK_NULL_HANDLE;
    }
    
    // Clean up descriptor resources
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        loader->vkDestroyDescriptorPool(context->getDevice(), computeDescriptorPool, nullptr);
        computeDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        loader->vkDestroyDescriptorSetLayout(context->getDevice(), computeDescriptorSetLayout, nullptr);
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
    GPUEntity* targetBufferPtr = static_cast<GPUEntity*>(entityBufferMapped);
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

void GPUEntityManager::updateAllMovementTypes(int newMovementType, bool angelMode) {
    if (activeEntityCount == 0) {
        return;
    }
    
    // Force GPU synchronization to ensure no buffers are being actively processed
    // This gives us absolute control over buffer contents
    if (context && loader) {
        loader->vkDeviceWaitIdle(context->getDevice());
    }
    
    // Update single buffer entities
    GPUEntity* bufferPtr = static_cast<GPUEntity*>(entityBufferMapped);
    
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
    if (!VulkanUtils::createBuffer(
        context->getDevice(),
        *loader,
        ENTITY_BUFFER_SIZE,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        entityStorage,
        entityMemory)) {
        std::cerr << "Failed to create entity buffer!" << std::endl;
        return false;
    }
    
    // Map memory for CPU access
    if (loader->vkMapMemory(context->getDevice(), entityMemory, 0, ENTITY_BUFFER_SIZE, 0, &entityBufferMapped) != VK_SUCCESS) {
        std::cerr << "Failed to map entity buffer memory!" << std::endl;
        return false;
    }
    
    return true;
}

bool GPUEntityManager::createComputeDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2; // 1 set * 2 buffers (input/output)
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1; // Single descriptor set
    
    return loader->vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &computeDescriptorPool) == VK_SUCCESS;
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
    
    return loader->vkCreateDescriptorSetLayout(context->getDevice(), &layoutInfo, nullptr, &computeDescriptorSetLayout) == VK_SUCCESS;
}

bool GPUEntityManager::createComputeDescriptorSets() {
    // Allocate single descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computeDescriptorSetLayout;
    
    if (loader->vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        return false;
    }
    
    // Update descriptor set - both input and output point to same buffer
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = entityStorage;
    bufferInfo.offset = 0;
    bufferInfo.range = ENTITY_BUFFER_SIZE;
    
    VkWriteDescriptorSet descriptorWrites[2] = {};
    
    // Input buffer (binding 0)
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = computeDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    
    // Output buffer (binding 1) - same buffer for in-place operations
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = computeDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfo;
    
    loader->vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites, 0, nullptr);
    
    return true;
}

// Note: findMemoryType and loadFunctions removed - now using VulkanUtils and VulkanFunctionLoader