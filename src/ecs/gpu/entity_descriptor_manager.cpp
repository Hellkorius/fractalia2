#include "entity_descriptor_manager.h"
#include "entity_buffer_manager.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include "../../vulkan/resources/descriptors/descriptor_update_helper.h"
#include "../../vulkan/resources/managers/descriptor_pool_manager.h"
#include <iostream>
#include <array>

EntityDescriptorManager::EntityDescriptorManager() {
}

bool EntityDescriptorManager::initializeEntity(EntityBufferManager& bufferManager, ResourceCoordinator* resourceCoordinator) {
    this->bufferManager = &bufferManager;
    this->resourceCoordinator = resourceCoordinator;
    
    if (!createDescriptorSetLayouts()) {
        std::cerr << "EntityDescriptorManager: Failed to create descriptor set layouts" << std::endl;
        return false;
    }
    
    std::cout << "EntityDescriptorManager: Initialized successfully with descriptor layouts" << std::endl;
    return true;
}

bool EntityDescriptorManager::initializeSpecialized() {
    // Base class handles common initialization
    // Entity-specific initialization is done in initializeEntity()
    return true;
}

void EntityDescriptorManager::cleanupSpecialized() {
    // Cleanup entity-specific resources
    computeDescriptorPool.reset();
    graphicsDescriptorPool.reset();
    
    cleanupDescriptorSetLayouts();
    
    bufferManager = nullptr;
    resourceCoordinator = nullptr;
    
    computeDescriptorSet = VK_NULL_HANDLE;
    graphicsDescriptorSet = VK_NULL_HANDLE;
}


void EntityDescriptorManager::cleanupDescriptorSetLayouts() {
    if (!validateContext()) return;
    
    const auto& loader = getContext()->getLoader();
    VkDevice device = getContext()->getDevice();
    
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
    if (!validateContext()) {
        return false;
    }

    const auto& loader = getContext()->getLoader();
    VkDevice device = getContext()->getDevice();

    // Create compute descriptor set layout for SoA structure
    VkDescriptorSetLayoutBinding computeBindings[7] = {};
    
    // Binding 0: Velocity buffer
    computeBindings[0].binding = 0;
    computeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[0].descriptorCount = 1;
    computeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Movement params buffer (for movement shader) / Runtime state buffer (for physics shader)
    computeBindings[1].binding = 1;
    computeBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[1].descriptorCount = 1;
    computeBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Runtime state buffer (for movement shader) / Position output buffer (for physics shader)
    computeBindings[2].binding = 2;
    computeBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[2].descriptorCount = 1;
    computeBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Current position buffer (for physics shader)
    computeBindings[3].binding = 3;
    computeBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[3].descriptorCount = 1;
    computeBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: Target position buffer (for physics shader)
    computeBindings[4].binding = 4;
    computeBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[4].descriptorCount = 1;
    computeBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: Color buffer (for graphics shader)
    computeBindings[5].binding = 5;
    computeBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[5].descriptorCount = 1;
    computeBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: Model matrix buffer (for graphics shader)
    computeBindings[6].binding = 6;
    computeBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[6].descriptorCount = 1;
    computeBindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeLayoutInfo.bindingCount = 7;  // Updated for SoA structure
    computeLayoutInfo.pBindings = computeBindings;

    if (loader.vkCreateDescriptorSetLayout(device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: Failed to create compute descriptor set layout" << std::endl;
        return false;
    }

    // Temporarily revert to simple graphics descriptor set layout for debugging
    VkDescriptorSetLayoutBinding graphicsBindings[3] = {};
    
    // Binding 0: Uniform buffer (camera matrices) 
    graphicsBindings[0].binding = 0;
    graphicsBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    graphicsBindings[0].descriptorCount = 1;
    graphicsBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Position buffer (for basic rendering test)
    graphicsBindings[1].binding = 1;
    graphicsBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    graphicsBindings[1].descriptorCount = 1;
    graphicsBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 2: Movement params buffer (for basic rendering test)
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
    DescriptorPoolManager::DescriptorPoolConfig config;
    config.maxSets = 1;
    config.uniformBuffers = 0;
    config.storageBuffers = 7; // 7 SoA storage buffers for compute
    config.sampledImages = 0;
    config.storageImages = 0;
    config.samplers = 0;
    config.allowFreeDescriptorSets = false; // Single-use pool

    computeDescriptorPool = getPoolManager().createDescriptorPool(config);
    return static_cast<bool>(computeDescriptorPool);
}

bool EntityDescriptorManager::createGraphicsDescriptorPool() {
    DescriptorPoolManager::DescriptorPoolConfig config;
    config.maxSets = 1;
    config.uniformBuffers = 1;  // Camera matrices
    config.storageBuffers = 2;  // Movement params + position buffers
    config.sampledImages = 0;
    config.storageImages = 0;
    config.samplers = 0;
    config.allowFreeDescriptorSets = false; // Single-use pool

    graphicsDescriptorPool = getPoolManager().createDescriptorPool(config);
    return static_cast<bool>(graphicsDescriptorPool);
}

bool EntityDescriptorManager::createComputeDescriptorSets(VkDescriptorSetLayout layout) {
    // Create descriptor pool if not already created
    if (!computeDescriptorPool && !createComputeDescriptorPool()) {
        std::cerr << "EntityDescriptorManager: Failed to create compute descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (getContext()->getLoader().vkAllocateDescriptorSets(getContext()->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
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
    if (!graphicsDescriptorPool && !createGraphicsDescriptorPool()) {
        std::cerr << "EntityDescriptorManager: Failed to create graphics descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (getContext()->getLoader().vkAllocateDescriptorSets(getContext()->getDevice(), &allocInfo, &graphicsDescriptorSet) != VK_SUCCESS) {
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

    // Use DescriptorUpdateHelper for DRY principle
    std::vector<DescriptorUpdateHelper::BufferBinding> bindings = {
        {0, bufferManager->getVelocityBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {1, bufferManager->getMovementParamsBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {2, bufferManager->getRuntimeStateBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {3, bufferManager->getPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {4, bufferManager->getCurrentPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {5, bufferManager->getColorBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {6, bufferManager->getModelMatrixBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}
    };

    return DescriptorUpdateHelper::updateDescriptorSet(*getContext(), computeDescriptorSet, bindings);
}

bool EntityDescriptorManager::updateGraphicsDescriptorSet() {
    if (!bufferManager) {
        std::cerr << "EntityDescriptorManager: Buffer manager not available" << std::endl;
        return false;
    }

    if (!resourceCoordinator) {
        std::cerr << "EntityDescriptorManager: ResourceCoordinator not available for uniform buffer binding" << std::endl;
        return false;
    }

    // Get uniform buffer from ResourceCoordinator
    const auto& uniformBuffers = resourceCoordinator->getUniformBuffers();
    if (uniformBuffers.empty()) {
        std::cerr << "EntityDescriptorManager: No uniform buffers available from ResourceCoordinator" << std::endl;
        return false;
    }

    // Use DescriptorUpdateHelper for DRY principle
    std::vector<DescriptorUpdateHelper::BufferBinding> bindings = {
        {0, uniformBuffers[0], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},  // Camera matrices
        {1, bufferManager->getPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},  // Entity positions
        {2, bufferManager->getMovementParamsBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}  // Movement params for color
    };

    return DescriptorUpdateHelper::updateDescriptorSet(*getContext(), graphicsDescriptorSet, bindings);
}

bool EntityDescriptorManager::recreateDescriptorSets() {
    // Recreate both compute and graphics descriptor sets
    bool computeSuccess = true;
    bool graphicsSuccess = true;
    
    // Recreate compute descriptor sets
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        computeSuccess = recreateComputeDescriptorSets();
    }
    
    // Recreate graphics descriptor sets
    if (graphicsDescriptorSetLayout != VK_NULL_HANDLE) {
        graphicsSuccess = recreateGraphicsDescriptorSets();
    }
    
    return computeSuccess && graphicsSuccess;
}

bool EntityDescriptorManager::recreateComputeDescriptorSets() {
    // Validate that we have the compute descriptor set layout
    if (computeDescriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Cannot recreate compute descriptor sets: layout not available" << std::endl;
        return false;
    }
    
    // Validate buffers are available
    if (!bufferManager || bufferManager->getVelocityBuffer() == VK_NULL_HANDLE || 
        bufferManager->getPositionBuffer() == VK_NULL_HANDLE || 
        bufferManager->getCurrentPositionBuffer() == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Cannot recreate compute descriptor sets: buffers not available" << std::endl;
        return false;
    }
    
    // Reset the descriptor pool to allow reallocation
    if (computeDescriptorPool) {
        if (getContext()->getLoader().vkResetDescriptorPool(getContext()->getDevice(), computeDescriptorPool.get(), 0) != VK_SUCCESS) {
            std::cerr << "EntityDescriptorManager: ERROR - Failed to reset compute descriptor pool" << std::endl;
            return false;
        }
        std::cout << "EntityDescriptorManager: Reset compute descriptor pool for reallocation" << std::endl;
        computeDescriptorSet = VK_NULL_HANDLE; // Pool reset invalidates all descriptor sets
    }
    
    // Allocate new descriptor set (reusing same layout and pool)
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computeDescriptorSetLayout;

    if (getContext()->getLoader().vkAllocateDescriptorSets(getContext()->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to reallocate compute descriptor set" << std::endl;
        return false;
    }

    if (!updateComputeDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to update recreated compute descriptor set" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Compute descriptor sets successfully recreated" << std::endl;
    return true;
}

bool EntityDescriptorManager::recreateGraphicsDescriptorSets() {
    // Similar logic for graphics descriptor sets
    if (graphicsDescriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Cannot recreate graphics descriptor sets: layout not available" << std::endl;
        return false;
    }
    
    if (!bufferManager || !resourceCoordinator) {
        std::cerr << "EntityDescriptorManager: ERROR - Cannot recreate graphics descriptor sets: dependencies not available" << std::endl;
        return false;
    }
    
    // Reset the descriptor pool
    if (graphicsDescriptorPool) {
        if (getContext()->getLoader().vkResetDescriptorPool(getContext()->getDevice(), graphicsDescriptorPool.get(), 0) != VK_SUCCESS) {
            std::cerr << "EntityDescriptorManager: ERROR - Failed to reset graphics descriptor pool" << std::endl;
            return false;
        }
        graphicsDescriptorSet = VK_NULL_HANDLE;
    }
    
    // Allocate new descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &graphicsDescriptorSetLayout;

    if (getContext()->getLoader().vkAllocateDescriptorSets(getContext()->getDevice(), &allocInfo, &graphicsDescriptorSet) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to reallocate graphics descriptor set" << std::endl;
        return false;
    }

    if (!updateGraphicsDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to update recreated graphics descriptor set" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Graphics descriptor sets successfully recreated" << std::endl;
    return true;
}