#include "entity_descriptor_manager.h"
#include "entity_buffer_manager.h"
#include "../vulkan/core/vulkan_context.h"
#include "../vulkan/core/vulkan_function_loader.h"
#include <iostream>
#include <array>

EntityDescriptorManager::EntityDescriptorManager() {
}

EntityDescriptorManager::~EntityDescriptorManager() {
    cleanup();
}

bool EntityDescriptorManager::initialize(const VulkanContext& context, EntityBufferManager& bufferManager) {
    this->context = &context;
    this->bufferManager = &bufferManager;
    
    if (!createDescriptorSetLayouts()) {
        std::cerr << "EntityDescriptorManager: Failed to create descriptor set layouts" << std::endl;
        return false;
    }
    
    std::cout << "EntityDescriptorManager: Initialized successfully with descriptor layouts" << std::endl;
    return true;
}

void EntityDescriptorManager::cleanup() {
    if (!context) return;
    
    cleanupDescriptorPools();
    cleanupDescriptorSetLayouts();
    
    context = nullptr;
    bufferManager = nullptr;
}

void EntityDescriptorManager::cleanupDescriptorPools() {
    const auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        computeDescriptorPool = VK_NULL_HANDLE;
        computeDescriptorSet = VK_NULL_HANDLE;
    }
    
    if (graphicsDescriptorPool != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorPool(device, graphicsDescriptorPool, nullptr);
        graphicsDescriptorPool = VK_NULL_HANDLE;
        graphicsDescriptorSet = VK_NULL_HANDLE;
    }
}

void EntityDescriptorManager::cleanupDescriptorSetLayouts() {
    const auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
        computeDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (graphicsDescriptorSetLayout != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);
        graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

bool EntityDescriptorManager::createDescriptorSetLayouts() {
    if (!context) {
        std::cerr << "EntityDescriptorManager: Context not initialized" << std::endl;
        return false;
    }

    const auto& loader = context->getLoader();
    VkDevice device = context->getDevice();

    // Create compute descriptor set layout
    VkDescriptorSetLayoutBinding computeBindings[4] = {};
    
    // Binding 0: Entity buffer
    computeBindings[0].binding = 0;
    computeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[0].descriptorCount = 1;
    computeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Position output buffer
    computeBindings[1].binding = 1;
    computeBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[1].descriptorCount = 1;
    computeBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Current position buffer
    computeBindings[2].binding = 2;
    computeBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[2].descriptorCount = 1;
    computeBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Target position buffer
    computeBindings[3].binding = 3;
    computeBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[3].descriptorCount = 1;
    computeBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeLayoutInfo.bindingCount = 4;
    computeLayoutInfo.pBindings = computeBindings;

    if (loader.vkCreateDescriptorSetLayout(device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: Failed to create compute descriptor set layout" << std::endl;
        return false;
    }

    // Create graphics descriptor set layout
    VkDescriptorSetLayoutBinding graphicsBindings[3] = {};
    
    // Binding 0: Uniform buffer (camera matrices)
    graphicsBindings[0].binding = 0;
    graphicsBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    graphicsBindings[0].descriptorCount = 1;
    graphicsBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Entity buffer
    graphicsBindings[1].binding = 1;
    graphicsBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    graphicsBindings[1].descriptorCount = 1;
    graphicsBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 2: Position buffer
    graphicsBindings[2].binding = 2;
    graphicsBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    graphicsBindings[2].descriptorCount = 1;
    graphicsBindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo{};
    graphicsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    graphicsLayoutInfo.bindingCount = 3;
    graphicsLayoutInfo.pBindings = graphicsBindings;

    if (loader.vkCreateDescriptorSetLayout(device, &graphicsLayoutInfo, nullptr, &graphicsDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: Failed to create graphics descriptor set layout" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Descriptor set layouts created successfully" << std::endl;
    return true;
}

bool EntityDescriptorManager::createComputeDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 4; // 4 storage buffers for compute

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1; // Only one descriptor set needed

    return context->getLoader().vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &computeDescriptorPool) == VK_SUCCESS;
}

bool EntityDescriptorManager::createGraphicsDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},       // Camera matrices
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}        // Entity buffer + position buffer
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    return context->getLoader().vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &graphicsDescriptorPool) == VK_SUCCESS;
}

bool EntityDescriptorManager::createComputeDescriptorSets(VkDescriptorSetLayout layout) {
    // Create descriptor pool if not already created
    if (computeDescriptorPool == VK_NULL_HANDLE && !createComputeDescriptorPool()) {
        std::cerr << "EntityDescriptorManager: Failed to create compute descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: Failed to allocate compute descriptor sets" << std::endl;
        return false;
    }

    if (!updateComputeDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: Failed to update compute descriptor set" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Compute descriptor sets created and updated" << std::endl;
    return true;
}

bool EntityDescriptorManager::createGraphicsDescriptorSets(VkDescriptorSetLayout layout) {
    // Create graphics descriptor pool
    if (graphicsDescriptorPool == VK_NULL_HANDLE && !createGraphicsDescriptorPool()) {
        std::cerr << "EntityDescriptorManager: Failed to create graphics descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &graphicsDescriptorSet) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: Failed to allocate graphics descriptor sets" << std::endl;
        return false;
    }

    if (!updateGraphicsDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: Failed to update graphics descriptor set" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Graphics descriptor sets created and updated" << std::endl;
    return true;
}

bool EntityDescriptorManager::updateComputeDescriptorSet() {
    if (!bufferManager) {
        std::cerr << "EntityDescriptorManager: Buffer manager not available" << std::endl;
        return false;
    }

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    // Binding 0: Entity buffer
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = bufferManager->getEntityBuffer();
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = computeDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &entityBufferInfo;

    // Binding 1: Position buffer (output)
    VkDescriptorBufferInfo positionBufferInfo{};
    positionBufferInfo.buffer = bufferManager->getPositionBuffer();
    positionBufferInfo.offset = 0;
    positionBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = computeDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &positionBufferInfo;

    // Binding 2: Current position buffer
    VkDescriptorBufferInfo currentPosBufferInfo{};
    currentPosBufferInfo.buffer = bufferManager->getCurrentPositionBuffer();
    currentPosBufferInfo.offset = 0;
    currentPosBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = computeDescriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &currentPosBufferInfo;

    // Binding 3: Target position buffer
    VkDescriptorBufferInfo targetPosBufferInfo{};
    targetPosBufferInfo.buffer = bufferManager->getTargetPositionBuffer();
    targetPosBufferInfo.offset = 0;
    targetPosBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = computeDescriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &targetPosBufferInfo;

    context->getLoader().vkUpdateDescriptorSets(context->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    return true;
}

bool EntityDescriptorManager::updateGraphicsDescriptorSet() {
    if (!bufferManager) {
        std::cerr << "EntityDescriptorManager: Buffer manager not available" << std::endl;
        return false;
    }

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    // Binding 1: Entity buffer
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = bufferManager->getEntityBuffer();
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = graphicsDescriptorSet;
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &entityBufferInfo;

    // Binding 2: Position buffer
    VkDescriptorBufferInfo positionBufferInfo{};
    positionBufferInfo.buffer = bufferManager->getPositionBuffer();
    positionBufferInfo.offset = 0;
    positionBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = graphicsDescriptorSet;
    descriptorWrites[1].dstBinding = 2;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &positionBufferInfo;

    context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites.data(), 0, nullptr);
    return true;
}

bool EntityDescriptorManager::recreateComputeDescriptorSets() {
    // CRITICAL FIX: Recreate compute descriptor sets during swapchain recreation
    // This is essential because compute descriptor sets can become stale during swapchain recreation
    
    // Validate that we have the compute descriptor set layout
    if (computeDescriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Cannot recreate compute descriptor sets: layout not available" << std::endl;
        return false;
    }
    
    // Validate buffers are available
    if (!bufferManager || bufferManager->getEntityBuffer() == VK_NULL_HANDLE || 
        bufferManager->getPositionBuffer() == VK_NULL_HANDLE || 
        bufferManager->getCurrentPositionBuffer() == VK_NULL_HANDLE || 
        bufferManager->getTargetPositionBuffer() == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Cannot recreate compute descriptor sets: buffers not available" << std::endl;
        return false;
    }
    
    // CRITICAL FIX: Reset the descriptor pool to allow reallocation
    // The pool was created with maxSets=1, so we need to reset it to reallocate
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        if (context->getLoader().vkResetDescriptorPool(context->getDevice(), computeDescriptorPool, 0) != VK_SUCCESS) {
            std::cerr << "EntityDescriptorManager: ERROR - Failed to reset compute descriptor pool" << std::endl;
            return false;
        }
        std::cout << "EntityDescriptorManager: Reset compute descriptor pool for reallocation" << std::endl;
        computeDescriptorSet = VK_NULL_HANDLE; // Pool reset invalidates all descriptor sets
    }
    
    // Allocate new descriptor set (reusing same layout and pool)
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computeDescriptorSetLayout;

    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to reallocate compute descriptor set" << std::endl;
        return false;
    }

    if (!updateComputeDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to update recreated compute descriptor set" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Compute descriptor sets successfully recreated during swapchain recreation" << std::endl;
    return true;
}