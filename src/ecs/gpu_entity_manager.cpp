#include "gpu_entity_manager.h"
#include "../vulkan/vulkan_context.h"
#include "../vulkan/vulkan_sync.h"
#include "../vulkan/vulkan_function_loader.h"
#include "../vulkan/vulkan_utils.h"
#include <iostream>
#include <cstring>
#include <algorithm>

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
    
    // Clean up entity buffers
    for (int i = 0; i < 2; i++) {
        if (entityBufferMapped[i] != nullptr) {
            loader->vkUnmapMemory(context->getDevice(), entityBufferMemory[i]);
            entityBufferMapped[i] = nullptr;
        }
        
        if (entityBuffers[i] != VK_NULL_HANDLE) {
            loader->vkDestroyBuffer(context->getDevice(), entityBuffers[i], nullptr);
            entityBuffers[i] = VK_NULL_HANDLE;
        }
        
        if (entityBufferMemory[i] != VK_NULL_HANDLE) {
            loader->vkFreeMemory(context->getDevice(), entityBufferMemory[i], nullptr);
            entityBufferMemory[i] = VK_NULL_HANDLE;
        }
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
    
    if (newEntityCount == 0) {
        std::cerr << "CRITICAL: GPU entity buffer full! Cannot upload " << requestedCount << " entities." << std::endl;
        std::cerr << "  Active: " << activeEntityCount << "/" << MAX_ENTITIES << " entities" << std::endl;
        std::cerr << "  This may cause entity creation failures!" << std::endl;
        return;
    }
    
    if (newEntityCount < requestedCount) {
        std::cerr << "WARNING: GPU buffer capacity exceeded!" << std::endl;
        std::cerr << "  Requested: " << requestedCount << ", Available: " << availableSpace << std::endl;
        std::cerr << "  Only uploading " << newEntityCount << " entities to prevent overflow." << std::endl;
    }
    
    // Copy to both buffers (current and next) with bounds validation
    for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++) {
        GPUEntity* bufferPtr = static_cast<GPUEntity*>(entityBufferMapped[bufferIndex]);
        
        // Final safety check before memory copy
        if (activeEntityCount + newEntityCount > MAX_ENTITIES) {
            std::cerr << "FATAL: Buffer overflow detected before GPU memory copy!" << std::endl;
            std::cerr << "  Active: " << activeEntityCount << ", New: " << newEntityCount 
                      << ", Max: " << MAX_ENTITIES << std::endl;
            return; // Abort upload to prevent corruption
        }
        
        // Validate buffer pointer
        if (!bufferPtr) {
            std::cerr << "FATAL: GPU entity buffer " << bufferIndex << " is null!" << std::endl;
            return;
        }
        
        std::memcpy(
            bufferPtr + activeEntityCount,
            pendingEntities.data(),
            newEntityCount * sizeof(GPUEntity)
        );
    }
    
    activeEntityCount += newEntityCount;
    
    // Clear uploaded entities
    if (newEntityCount == pendingEntities.size()) {
        pendingEntities.clear();
    } else {
        pendingEntities.erase(pendingEntities.begin(), pendingEntities.begin() + newEntityCount);
    }
    
    std::cout << "Uploaded " << newEntityCount << " entities to GPU. Total: " << activeEntityCount << std::endl;
}

void GPUEntityManager::clearAllEntities() {
    activeEntityCount = 0;
    pendingEntities.clear();
}

void GPUEntityManager::updateAllMovementTypes(int newMovementType, bool angelMode) {
    if (activeEntityCount == 0) {
        return;
    }
    
    // Update both buffers to keep them in sync
    for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++) {
        GPUEntity* bufferPtr = static_cast<GPUEntity*>(entityBufferMapped[bufferIndex]);
        
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
    for (int i = 0; i < 2; i++) {
        if (!VulkanUtils::createBuffer(
            context->getDevice(),
            *loader,
            ENTITY_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            entityBuffers[i],
            entityBufferMemory[i])) {
            std::cerr << "Failed to create entity buffer " << i << "!" << std::endl;
            return false;
        }
        
        // Map memory for CPU access
        if (loader->vkMapMemory(context->getDevice(), entityBufferMemory[i], 0, ENTITY_BUFFER_SIZE, 0, &entityBufferMapped[i]) != VK_SUCCESS) {
            std::cerr << "Failed to map entity buffer memory " << i << "!" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool GPUEntityManager::createComputeDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 4; // 2 sets * 2 buffers each
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2; // One set per double buffer
    
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
    // Allocate descriptor sets
    VkDescriptorSetLayout layouts[2] = {computeDescriptorSetLayout, computeDescriptorSetLayout};
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;
    
    if (loader->vkAllocateDescriptorSets(context->getDevice(), &allocInfo, computeDescriptorSets) != VK_SUCCESS) {
        return false;
    }
    
    // Update descriptor sets for double buffering
    for (int i = 0; i < 2; i++) {
        VkDescriptorBufferInfo inputBufferInfo{};
        inputBufferInfo.buffer = entityBuffers[i];
        inputBufferInfo.offset = 0;
        inputBufferInfo.range = ENTITY_BUFFER_SIZE;
        
        VkDescriptorBufferInfo outputBufferInfo{};
        outputBufferInfo.buffer = entityBuffers[1 - i]; // Opposite buffer
        outputBufferInfo.offset = 0;
        outputBufferInfo.range = ENTITY_BUFFER_SIZE;
        
        VkWriteDescriptorSet descriptorWrites[2] = {};
        
        // Input buffer
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = computeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &inputBufferInfo;
        
        // Output buffer
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = computeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &outputBufferInfo;
        
        loader->vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

// Note: findMemoryType and loadFunctions removed - now using VulkanUtils and VulkanFunctionLoader