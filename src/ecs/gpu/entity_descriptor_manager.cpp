#include "entity_descriptor_manager.h"
#include "entity_buffer_manager.h"
#include "entity_descriptor_bindings.h"
#include "entity_buffer_types.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include "../../vulkan/resources/descriptors/descriptor_update_helper.h"
#include "../../vulkan/resources/managers/descriptor_pool_manager.h"
#include "../../vulkan/pipelines/descriptor_layout_manager.h"
#include <iostream>
#include <array>

EntityDescriptorManager::EntityDescriptorManager() {
}

bool EntityDescriptorManager::initializeEntity(EntityBufferManager& bufferManager, ResourceCoordinator* resourceCoordinator) {
    this->bufferManager = &bufferManager;
    this->resourceCoordinator = resourceCoordinator;
    
    // Use Vulkan 1.3 descriptor indexing
    if (!createIndexedDescriptorSetLayout()) {
        std::cerr << "EntityDescriptorManager: Failed to create indexed descriptor set layout" << std::endl;
        return false;
    }
    
    if (!createIndexedDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: Failed to create indexed descriptor set" << std::endl;
        return false;
    }
    
    std::cout << "EntityDescriptorManager: Initialized successfully with indexed descriptor system" << std::endl;
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
    indexedDescriptorPool.reset();
    
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

    // Binding 2: Movement centers buffer (for 3D movement centers)
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_CENTERS_BUFFER].binding = EntityDescriptorBindings::Compute::MOVEMENT_CENTERS_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_CENTERS_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_CENTERS_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::MOVEMENT_CENTERS_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Runtime state buffer (for movement shader) / Position output buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].binding = EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: Current position buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].binding = EntityDescriptorBindings::Compute::POSITION_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::POSITION_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: Target position buffer (for physics shader)
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].binding = EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: Rotation state buffer (for movement and physics shaders)
    computeBindings[EntityDescriptorBindings::Compute::ROTATION_STATE_BUFFER].binding = EntityDescriptorBindings::Compute::ROTATION_STATE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::ROTATION_STATE_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::ROTATION_STATE_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::ROTATION_STATE_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 7: Color buffer (for graphics shader)
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].binding = EntityDescriptorBindings::Compute::COLOR_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::COLOR_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 8: Model matrix buffer (for graphics shader)
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].binding = EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].descriptorCount = 1;
    computeBindings[EntityDescriptorBindings::Compute::MODEL_MATRIX_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 9: Spatial map buffer (for physics shader)
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

    // Binding 3: Rotation state buffer (for entity rotation rendering)
    graphicsBindings[EntityDescriptorBindings::Graphics::ROTATION_STATE_BUFFER].binding = EntityDescriptorBindings::Graphics::ROTATION_STATE_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::ROTATION_STATE_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    graphicsBindings[EntityDescriptorBindings::Graphics::ROTATION_STATE_BUFFER].descriptorCount = 1;
    graphicsBindings[EntityDescriptorBindings::Graphics::ROTATION_STATE_BUFFER].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

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
        {EntityDescriptorBindings::Compute::MOVEMENT_CENTERS_BUFFER, bufferManager->getMovementCentersBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::RUNTIME_STATE_BUFFER, bufferManager->getRuntimeStateBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::POSITION_BUFFER, bufferManager->getPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::CURRENT_POSITION_BUFFER, bufferManager->getCurrentPositionBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {EntityDescriptorBindings::Compute::ROTATION_STATE_BUFFER, bufferManager->getRotationStateBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
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
        {EntityDescriptorBindings::Graphics::MOVEMENT_PARAMS_BUFFER, bufferManager->getMovementParamsBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},  // Movement params for color
        {EntityDescriptorBindings::Graphics::ROTATION_STATE_BUFFER, bufferManager->getRotationStateBuffer(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}  // Rotation state for transforms
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

// Vulkan 1.3 descriptor indexing implementation
bool EntityDescriptorManager::createIndexedDescriptorSetLayout() {
    if (!getContext()) {
        std::cerr << "EntityDescriptorManager: ERROR - VulkanContext is null" << std::endl;
        return false;
    }

    // Use the new indexed layout from DescriptorLayoutPresets
    try {
        auto spec = DescriptorLayoutPresets::createEntityIndexedLayout();
        
        DescriptorLayoutManager layoutManager;
        layoutManager.initialize(*getContext());
        
        VkDescriptorSetLayout layout = layoutManager.createLayout(spec);
        if (layout == VK_NULL_HANDLE) {
            std::cerr << "EntityDescriptorManager: ERROR - Failed to create indexed descriptor set layout" << std::endl;
            return false;
        }
        
        indexedDescriptorSetLayout = layout;
        
        std::cout << "EntityDescriptorManager: Indexed descriptor set layout created successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "EntityDescriptorManager: ERROR - Exception creating indexed layout: " << e.what() << std::endl;
        return false;
    }
}

bool EntityDescriptorManager::createIndexedDescriptorSet() {
    if (!getContext()) {
        std::cerr << "EntityDescriptorManager: ERROR - VulkanContext is null" << std::endl;
        return false;
    }
    
    if (indexedDescriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Indexed descriptor set layout not created" << std::endl;
        return false;
    }

    // Create descriptor pool for indexed descriptors
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}); // For camera matrices
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, EntityBufferType::MAX_ENTITY_BUFFERS + 1}); // +1 for spatial map

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; // Required for descriptor indexing
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    indexedDescriptorPool = vulkan_raii::create_descriptor_pool(getContext(), &poolInfo);
    if (!indexedDescriptorPool.get()) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to create indexed descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = indexedDescriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &indexedDescriptorSetLayout;

    if (getContext()->getLoader().vkAllocateDescriptorSets(getContext()->getDevice(), &allocInfo, &indexedDescriptorSet) != VK_SUCCESS) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to allocate indexed descriptor set" << std::endl;
        return false;
    }

    if (!updateIndexedDescriptorSet()) {
        std::cerr << "EntityDescriptorManager: ERROR - Failed to update indexed descriptor set" << std::endl;
        return false;
    }

    std::cout << "EntityDescriptorManager: Indexed descriptor set created successfully" << std::endl;
    return true;
}

bool EntityDescriptorManager::updateIndexedDescriptorSet() {
    if (!bufferManager) {
        std::cerr << "EntityDescriptorManager: ERROR - Buffer manager is null" << std::endl;
        return false;
    }
    
    if (!resourceCoordinator) {
        std::cerr << "EntityDescriptorManager: ERROR - ResourceCoordinator not available for uniform buffer binding" << std::endl;
        return false;
    }
    
    if (indexedDescriptorSet == VK_NULL_HANDLE) {
        std::cerr << "EntityDescriptorManager: ERROR - Indexed descriptor set is null" << std::endl;
        return false;
    }

    // Update all descriptors in the indexed descriptor set
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos(EntityBufferType::MAX_ENTITY_BUFFERS);
    
    // Add uniform buffer binding at binding 0 (camera matrices)
    const auto& uniformBuffers = resourceCoordinator->getUniformBuffers();
    if (uniformBuffers.empty()) {
        std::cerr << "EntityDescriptorManager: ERROR - No uniform buffers available from ResourceCoordinator" << std::endl;
        return false;
    }
    
    VkDescriptorBufferInfo uniformBufferInfo = {uniformBuffers[0], 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet uniformWrite{};
    uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformWrite.dstSet = indexedDescriptorSet;
    uniformWrite.dstBinding = 0; // Camera matrices at binding 0
    uniformWrite.dstArrayElement = 0;
    uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformWrite.descriptorCount = 1;
    uniformWrite.pBufferInfo = &uniformBufferInfo;
    writes.push_back(uniformWrite);
    
    // Map buffer types to their corresponding buffers
    struct BufferMapping {
        uint32_t bufferType;
        VkBuffer buffer;
        const char* name;
    };
    
    std::vector<BufferMapping> bufferMappings = {
        {EntityBufferType::VELOCITY, bufferManager->getVelocityBuffer(), "VelocityBuffer"},
        {EntityBufferType::MOVEMENT_PARAMS, bufferManager->getMovementParamsBuffer(), "MovementParamsBuffer"},
        {EntityBufferType::RUNTIME_STATE, bufferManager->getRuntimeStateBuffer(), "RuntimeStateBuffer"},
        {EntityBufferType::ROTATION_STATE, bufferManager->getRotationStateBuffer(), "RotationStateBuffer"},
        {EntityBufferType::COLOR, bufferManager->getColorBuffer(), "ColorBuffer"},
        {EntityBufferType::MODEL_MATRIX, bufferManager->getModelMatrixBuffer(), "ModelMatrixBuffer"},
        {EntityBufferType::POSITION_OUTPUT, bufferManager->getPositionBuffer(), "PositionOutputBuffer"},
        {EntityBufferType::CURRENT_POSITION, bufferManager->getCurrentPositionBuffer(), "CurrentPositionBuffer"},
        {EntityBufferType::SPATIAL_MAP, bufferManager->getSpatialMapBuffer(), "SpatialMapBuffer"}
    };

    // Update each buffer in the indexed array
    for (const auto& mapping : bufferMappings) {
        if (mapping.buffer == VK_NULL_HANDLE) {
            std::cerr << "EntityDescriptorManager: WARNING - " << mapping.name << " is null, skipping" << std::endl;
            continue;
        }
        
        bufferInfos[mapping.bufferType] = {mapping.buffer, 0, VK_WHOLE_SIZE};
        
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = indexedDescriptorSet;
        write.dstBinding = 1; // Entity buffer array at binding 1
        write.dstArrayElement = mapping.bufferType; // Index into the buffer array
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos[mapping.bufferType];
        
        writes.push_back(write);
    }

    // Add spatial map buffer binding (binding 2)
    if (bufferManager->getSpatialMapBuffer() != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo spatialMapInfo = {bufferManager->getSpatialMapBuffer(), 0, VK_WHOLE_SIZE};
        
        VkWriteDescriptorSet spatialMapWrite{};
        spatialMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        spatialMapWrite.dstSet = indexedDescriptorSet;
        spatialMapWrite.dstBinding = 2; // Spatial map at binding 2
        spatialMapWrite.dstArrayElement = 0;
        spatialMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        spatialMapWrite.descriptorCount = 1;
        spatialMapWrite.pBufferInfo = &spatialMapInfo;
        
        writes.push_back(spatialMapWrite);
    }

    if (!writes.empty()) {
        getContext()->getLoader().vkUpdateDescriptorSets(
            getContext()->getDevice(),
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr
        );
        std::cout << "EntityDescriptorManager: Updated " << writes.size() << " indexed descriptors" << std::endl;
    }

    return true;
}