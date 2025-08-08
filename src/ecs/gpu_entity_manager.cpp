#include "gpu_entity_manager.h"
#include "../vulkan/vulkan_context.h"
#include "../vulkan/vulkan_sync.h"
#include <iostream>
#include <cstring>
#include <algorithm>

GPUEntityManager::GPUEntityManager() {
    pendingEntities.reserve(10000); // Pre-allocate for batch uploads
}

GPUEntityManager::~GPUEntityManager() {
    cleanup();
}

bool GPUEntityManager::initialize(VulkanContext* context, VulkanSync* sync) {
    this->context = context;
    this->sync = sync;
    
    loadFunctions();
    
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
    // Clean up entity buffers
    for (int i = 0; i < 2; i++) {
        if (entityBufferMapped[i] != nullptr) {
            vkUnmapMemory(context->getDevice(), entityBufferMemory[i]);
            entityBufferMapped[i] = nullptr;
        }
        
        if (entityBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(context->getDevice(), entityBuffers[i], nullptr);
            entityBuffers[i] = VK_NULL_HANDLE;
        }
        
        if (entityBufferMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(context->getDevice(), entityBufferMemory[i], nullptr);
            entityBufferMemory[i] = VK_NULL_HANDLE;
        }
    }
    
    // Clean up descriptor resources
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context->getDevice(), computeDescriptorPool, nullptr);
        computeDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context->getDevice(), computeDescriptorSetLayout, nullptr);
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
    
    uint32_t newEntityCount = std::min(
        static_cast<uint32_t>(pendingEntities.size()),
        MAX_ENTITIES - activeEntityCount
    );
    
    if (newEntityCount == 0) {
        std::cerr << "Warning: Cannot upload entities, buffer full!" << std::endl;
        return;
    }
    
    // Copy to both buffers (current and next)
    for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++) {
        GPUEntity* bufferPtr = static_cast<GPUEntity*>(entityBufferMapped[bufferIndex]);
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
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = ENTITY_BUFFER_SIZE;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &entityBuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create entity buffer " << i << "!" << std::endl;
            return false;
        }
        
        // Allocate memory
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(context->getDevice(), entityBuffers[i], &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &entityBufferMemory[i]) != VK_SUCCESS) {
            std::cerr << "Failed to allocate entity buffer memory " << i << "!" << std::endl;
            return false;
        }
        
        // Bind and map memory
        vkBindBufferMemory(context->getDevice(), entityBuffers[i], entityBufferMemory[i], 0);
        vkMapMemory(context->getDevice(), entityBufferMemory[i], 0, ENTITY_BUFFER_SIZE, 0, &entityBufferMapped[i]);
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
    
    return vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &computeDescriptorPool) == VK_SUCCESS;
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
    
    return vkCreateDescriptorSetLayout(context->getDevice(), &layoutInfo, nullptr, &computeDescriptorSetLayout) == VK_SUCCESS;
}

bool GPUEntityManager::createComputeDescriptorSets() {
    // Allocate descriptor sets
    VkDescriptorSetLayout layouts[2] = {computeDescriptorSetLayout, computeDescriptorSetLayout};
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;
    
    if (vkAllocateDescriptorSets(context->getDevice(), &allocInfo, computeDescriptorSets) != VK_SUCCESS) {
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
        
        vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

uint32_t GPUEntityManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

void GPUEntityManager::loadFunctions() {
    VkInstance instance = context->getInstance();
    
    vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(context->vkGetInstanceProcAddr(instance, "vkCreateBuffer"));
    vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(context->vkGetInstanceProcAddr(instance, "vkDestroyBuffer"));
    vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(context->vkGetInstanceProcAddr(instance, "vkGetBufferMemoryRequirements"));
    vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(context->vkGetInstanceProcAddr(instance, "vkBindBufferMemory"));
    vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(context->vkGetInstanceProcAddr(instance, "vkAllocateMemory"));
    vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(context->vkGetInstanceProcAddr(instance, "vkFreeMemory"));
    vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(context->vkGetInstanceProcAddr(instance, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(context->vkGetInstanceProcAddr(instance, "vkUnmapMemory"));
    vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(context->vkGetInstanceProcAddr(instance, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(context->vkGetInstanceProcAddr(instance, "vkDestroyDescriptorPool"));
    vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(context->vkGetInstanceProcAddr(instance, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(context->vkGetInstanceProcAddr(instance, "vkDestroyDescriptorSetLayout"));
    vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(context->vkGetInstanceProcAddr(instance, "vkAllocateDescriptorSets"));
    vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(context->vkGetInstanceProcAddr(instance, "vkUpdateDescriptorSets"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(context->vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
}