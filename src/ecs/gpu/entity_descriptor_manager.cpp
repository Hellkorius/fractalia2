#include "entity_descriptor_manager.h"
#include "entity_buffer_manager.h"
#include "entity_descriptor_bindings.h"
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
    VkDescriptorSetLayoutBinding computeBindings[EntityDescriptorBindings::Compute::BINDING_COUNT] = {};
    
    // Binding 0: Velocity buffer
    computeBindings[EntityDescriptorBindings::Compute::VELOCITY_BUFFER].binding = EntityDescriptorBindings::Compute::VELOCITY_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::VELOCITY_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::VELOCITY_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::VELOCITY_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Movement params buffer (for movement shader) / Runtime state buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_PARAMS_BUFFER].binding = EntityDescriptorBindings::Compute::MOVEMENT_PARAMS_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_PARAMS_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_PARAMS_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_PARAMS_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Runtime state buffer (for movement shader) / Position output buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].binding = EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Current position buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].binding = EntityDescriptorBindings::Compute::POSITION_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: Target position buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].binding = EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: Color buffer (for graphics shader)
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].binding = EntityDescriptorBindings::Compute::COLOR_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: Model matrix buffer (for graphics shader)
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].binding = EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 7: Spatial map buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::SPATIAL_MAP_BUFFER].binding = EntityDescriptorBindings::Compute::SPATIAL_MAP_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::SPATIAL_MAP_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::SPATIAL_MAP_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::SPATIAL_MAP_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeLayoutInfo.bindingCount = EntityDescriptorBindings::Compute::BINDING_COUNT;
    computeLayoutInfo.pBindings = computeBindings;

    if (loader.vkCreateDescriptorSetLayout(device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: Failed to create compute descriptor set layout" << std::endl;
        return false;
    }

    // Temporarily revert to simple graphics descriptor set layout for debugging
    VkDescriptorSetLayoutBinding graphicsBindings[EntityDescriptorBindings::Graphics::BINDING_COUNT] = {};
    
    // Binding 0: Uniform buffer (camera matrices) 
    graphicsBindings[EntityDescriptorBindings::Graphics::UNIFORM_BUFFER].binding = EntityDescriptorBindings::Graphics::UNIFORM_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::UNIFORM_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::UNIFORM_BUFFER].descriptorCount = 1;
    graphicsBindings[EntityDescriptorBindings::Graphics::UNIFORM_BUFFER].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Position buffer (for basic rendering test)
    graphicsBindings[EntityDescriptorBindings::Graphics::POSITION_BUFFER].binding = EntityDescriptorBindings::Graphics::POSITION_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::POSITION_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::POSITION_BUFFER].descriptorCount = 1;
    graphicsBindings[EntityDescriptorBindings::Graphics::POSITION_BUFFER].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 2: Movement params buffer (for basic rendering test)
    graphicsBindings[EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER].binding = EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER].descriptorCount = 1;
    graphicsBindings[EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo{};
    graphicsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    graphicsLayoutInfo.bindingCount = EntityDescriptorBindings::Graphics::BINDING_COUNT;
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
    config.storageBuffers = EntityDescriptorBindings::Compute::BINDING_COUNT; // SoA storage buffers for compute
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
    config.storageBuffers = EntityDescriptorBindings::Graphics::BINDING_COUNT - 1;  // Storage buffers (excluding uniform buffer)
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
        {EntityDescriptorBindings::Compute::VELOCITY_BUFFER, bufferManager->getVelocityBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::MOVEMENT_PARAMS_BUFFER, bufferManager->getMovementParamsBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER, bufferManager->getRuntimeStateBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::POSITION_BUFFER, bufferManager->getPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER, bufferManager->getCurrentPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::COLOR_BUFFER, bufferManager->getColorBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER, bufferManager->getModelMatrixBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::SPATIAL_MAP_BUFFER, bufferManager->getSpatialMapBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}
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
        {EntityDescriptorBindings::Graphics::UNIFORM_BUFFER, uniformBuffers[0], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},  // Camera matrices
        {EntityDescriptorBindings::Graphics::POSITION_BUFFER, bufferManager->getPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},  // Entity positions
        {EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER, bufferManager->getMovementParamsBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}  // Movement params for color
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