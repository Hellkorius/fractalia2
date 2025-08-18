#include "gpu_entity_manager.h"
#include "../vulkan/core/vulkan_context.h"
#include "../vulkan/core/vulkan_sync.h"
#include "../vulkan/resources/resource_context.h"
#include "../vulkan/core/vulkan_function_loader.h"
#include "../vulkan/core/vulkan_utils.h"
#include <iostream>
#include <cstring>
#include <random>
#include <array>

GPUEntity GPUEntity::fromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
    GPUEntity entity{};
    
    // Copy movement parameters
    entity.movementParams0 = glm::vec4(
        pattern.amplitude,
        pattern.frequency, 
        pattern.phase,
        pattern.timeOffset
    );
    
    entity.movementParams1 = glm::vec4(
        pattern.center.x,
        pattern.center.y, 
        pattern.center.z,
        static_cast<float>(pattern.type)
    );
    
    // Copy color
    entity.color = renderable.color;
    
    // Copy transform matrix
    entity.modelMatrix = transform.getMatrix();
    
    // Initialize runtime state with random staggering
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 600.0f);
    
    entity.runtimeState = glm::vec4(
        0.0f,           // totalTime (will be updated by compute shader)
        0.0f,           // initialized flag (must start as 0.0 for GPU initialization)
        dis(gen),       // stateTimer (random staggering)
        0.0f            // entityState (reserved)
    );
    
    return entity;
}

GPUEntityManager::GPUEntityManager() {
}

GPUEntityManager::~GPUEntityManager() {
    cleanup();
}

bool GPUEntityManager::initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext) {
    this->context = &context;
    this->sync = sync;
    this->resourceContext = resourceContext;
    
    // Create entity buffer
    if (!createBuffer(ENTITY_BUFFER_SIZE, 
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     entityBuffer, entityBufferMemory)) {
        std::cerr << "GPUEntityManager: Failed to create entity buffer" << std::endl;
        return false;
    }
    
    // Create position buffer
    if (!createBuffer(POSITION_BUFFER_SIZE,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     positionBuffer, positionBufferMemory)) {
        std::cerr << "GPUEntityManager: Failed to create position buffer" << std::endl;
        return false;
    }
    
    // Create alternate position buffer for async compute ping-pong
    if (!createBuffer(POSITION_BUFFER_SIZE,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     positionBufferAlternate, positionBufferAlternateMemory)) {
        std::cerr << "GPUEntityManager: Failed to create alternate position buffer" << std::endl;
        return false;
    }
    
    // Create current position buffer
    if (!createBuffer(POSITION_BUFFER_SIZE,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     currentPositionBuffer, currentPositionBufferMemory)) {
        std::cerr << "GPUEntityManager: Failed to create current position buffer" << std::endl;
        return false;
    }
    
    // Create target position buffer
    if (!createBuffer(POSITION_BUFFER_SIZE,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     targetPositionBuffer, targetPositionBufferMemory)) {
        std::cerr << "GPUEntityManager: Failed to create target position buffer" << std::endl;
        return false;
    }
    
    // Create descriptor set layouts for pipeline system integration
    if (!createDescriptorSetLayouts()) {
        std::cerr << "GPUEntityManager: Failed to create descriptor set layouts" << std::endl;
        return false;
    }
    
    std::cout << "GPUEntityManager: Initialized successfully with descriptor layouts" << std::endl;
    return true;
}

void GPUEntityManager::cleanup() {
    if (!context) return;
    
    const auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    // Cleanup descriptor resources
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        computeDescriptorPool = VK_NULL_HANDLE;
        computeDescriptorSet = VK_NULL_HANDLE; // Automatically freed with pool
    }
    
    if (graphicsDescriptorPool != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorPool(device, graphicsDescriptorPool, nullptr);
        graphicsDescriptorPool = VK_NULL_HANDLE;
        graphicsDescriptorSet = VK_NULL_HANDLE; // Automatically freed with pool
    }
    
    // Cleanup descriptor set layouts
    if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
        computeDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (graphicsDescriptorSetLayout != VK_NULL_HANDLE) {
        loader.vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);
        graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (entityBuffer != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, entityBuffer, nullptr);
        entityBuffer = VK_NULL_HANDLE;
    }
    if (entityBufferMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, entityBufferMemory, nullptr);
        entityBufferMemory = VK_NULL_HANDLE;
    }
    
    if (positionBuffer != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, positionBuffer, nullptr);
        positionBuffer = VK_NULL_HANDLE;
    }
    if (positionBufferMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, positionBufferMemory, nullptr);
        positionBufferMemory = VK_NULL_HANDLE;
    }
    
    if (positionBufferAlternate != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, positionBufferAlternate, nullptr);
        positionBufferAlternate = VK_NULL_HANDLE;
    }
    if (positionBufferAlternateMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, positionBufferAlternateMemory, nullptr);
        positionBufferAlternateMemory = VK_NULL_HANDLE;
    }
    
    if (currentPositionBuffer != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, currentPositionBuffer, nullptr);
        currentPositionBuffer = VK_NULL_HANDLE;
    }
    if (currentPositionBufferMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, currentPositionBufferMemory, nullptr);
        currentPositionBufferMemory = VK_NULL_HANDLE;
    }
    
    if (targetPositionBuffer != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, targetPositionBuffer, nullptr);
        targetPositionBuffer = VK_NULL_HANDLE;
    }
    if (targetPositionBufferMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, targetPositionBufferMemory, nullptr);
        targetPositionBufferMemory = VK_NULL_HANDLE;
    }
}

void GPUEntityManager::addEntity(const GPUEntity& entity) {
    if (activeEntityCount + stagingEntities.size() >= MAX_ENTITIES) {
        std::cerr << "GPUEntityManager: Cannot add entity, would exceed max capacity" << std::endl;
        return;
    }
    
    stagingEntities.push_back(entity);
}

void GPUEntityManager::addEntitiesFromECS(const std::vector<Entity>& entities) {
    for (const auto& entity : entities) {
        if (activeEntityCount + stagingEntities.size() >= MAX_ENTITIES) {
            std::cerr << "GPUEntityManager: Reached max capacity, stopping entity addition" << std::endl;
            break;
        }
        
        auto* transform = entity.get<Transform>();
        auto* renderable = entity.get<Renderable>();
        auto* pattern = entity.get<MovementPattern>();
        
        if (transform && renderable && pattern) {
            GPUEntity gpuEntity = GPUEntity::fromECS(*transform, *renderable, *pattern);
            stagingEntities.push_back(gpuEntity);
        }
    }
}

void GPUEntityManager::uploadPendingEntities() {
    if (stagingEntities.empty()) return;
    
    // Copy staging entities to GPU buffer
    VkDeviceSize uploadSize = stagingEntities.size() * sizeof(GPUEntity);
    VkDeviceSize offset = activeEntityCount * sizeof(GPUEntity);
    
    // Map memory and copy data
    void* data;
    if (context->getLoader().vkMapMemory(context->getDevice(), entityBufferMemory, offset, uploadSize, 0, &data) == VK_SUCCESS) {
        std::memcpy(data, stagingEntities.data(), uploadSize);
        context->getLoader().vkUnmapMemory(context->getDevice(), entityBufferMemory);
        
        activeEntityCount += stagingEntities.size();
        stagingEntities.clear();
        
        std::cout << "GPUEntityManager: Uploaded " << (uploadSize / sizeof(GPUEntity)) << " entities, total: " << activeEntityCount << std::endl;
    } else {
        std::cerr << "GPUEntityManager: Failed to map entity buffer memory" << std::endl;
    }
}

void GPUEntityManager::clearAllEntities() {
    stagingEntities.clear();
    activeEntityCount = 0;
}

VkBuffer GPUEntityManager::getEntityBuffer() const {
    return entityBuffer;
}

VkBuffer GPUEntityManager::getPositionBuffer() const {
    return positionBuffer;
}

VkBuffer GPUEntityManager::getCurrentPositionBuffer() const {
    return currentPositionBuffer;
}

VkBuffer GPUEntityManager::getTargetPositionBuffer() const {
    return targetPositionBuffer;
}

VkBuffer GPUEntityManager::getPositionBufferAlternate() const {
    return positionBufferAlternate;
}

VkBuffer GPUEntityManager::getComputeWriteBuffer(uint32_t frameIndex) const {
    // Ping-pong: even frames write to positionBuffer, odd frames write to positionBufferAlternate
    return (frameIndex % 2 == 0) ? positionBuffer : positionBufferAlternate;
}

VkBuffer GPUEntityManager::getGraphicsReadBuffer(uint32_t frameIndex) const {
    // Graphics reads from the buffer compute wrote to in the PREVIOUS frame
    // So if compute is writing to buffer A (frame N), graphics reads from buffer B (frame N-1)
    return (frameIndex % 2 == 0) ? positionBufferAlternate : positionBuffer;
}

bool GPUEntityManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (context->getLoader().vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    context->getLoader().vkGetBufferMemoryRequirements(context->getDevice(), buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(context->getPhysicalDevice(), context->getLoader(), memRequirements.memoryTypeBits, 
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        context->getLoader().vkDestroyBuffer(context->getDevice(), buffer, nullptr);
        return false;
    }
    
    context->getLoader().vkBindBufferMemory(context->getDevice(), buffer, bufferMemory, 0);
    return true;
}

bool GPUEntityManager::createComputeDescriptorPool() {
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

bool GPUEntityManager::createComputeDescriptorSets(VkDescriptorSetLayout layout) {
    // Create descriptor pool if not already created
    if (computeDescriptorPool == VK_NULL_HANDLE && !createComputeDescriptorPool()) {
        std::cerr << "GPUEntityManager: Failed to create compute descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        std::cerr << "GPUEntityManager: Failed to allocate compute descriptor sets" << std::endl;
        return false;
    }

    // Update descriptor set with buffer bindings
    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    // Binding 0: Entity buffer
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = entityBuffer;
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
    positionBufferInfo.buffer = positionBuffer;
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
    currentPosBufferInfo.buffer = currentPositionBuffer;
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
    targetPosBufferInfo.buffer = targetPositionBuffer;
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

    std::cout << "GPUEntityManager: Compute descriptor sets created and updated" << std::endl;
    return true;
}

bool GPUEntityManager::createGraphicsDescriptorSets(VkDescriptorSetLayout layout) {
    // Create graphics descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},       // Camera matrices
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}        // Entity buffer + position buffer
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (context->getLoader().vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &graphicsDescriptorPool) != VK_SUCCESS) {
        std::cerr << "GPUEntityManager: Failed to create graphics descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &graphicsDescriptorSet) != VK_SUCCESS) {
        std::cerr << "GPUEntityManager: Failed to allocate graphics descriptor sets" << std::endl;
        return false;
    }

    // Update descriptor set with buffer bindings
    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    // Binding 0: Uniform buffer (camera matrices - handled by resource context)
    // This will be updated externally

    // Binding 1: Entity buffer
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = entityBuffer;
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
    positionBufferInfo.buffer = positionBuffer;
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

    std::cout << "GPUEntityManager: Graphics descriptor sets created and updated" << std::endl;
    return true;
}

bool GPUEntityManager::createDescriptorSetLayouts() {
    if (!context) {
        std::cerr << "GPUEntityManager: Context not initialized" << std::endl;
        return false;
    }

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

    if (context->getLoader().vkCreateDescriptorSetLayout(context->getDevice(), &computeLayoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "GPUEntityManager: Failed to create compute descriptor set layout" << std::endl;
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

    if (context->getLoader().vkCreateDescriptorSetLayout(context->getDevice(), &graphicsLayoutInfo, nullptr, &graphicsDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "GPUEntityManager: Failed to create graphics descriptor set layout" << std::endl;
        return false;
    }

    std::cout << "GPUEntityManager: Descriptor set layouts created successfully" << std::endl;
    return true;
}

bool GPUEntityManager::recreateComputeDescriptorSets() {
    // CRITICAL FIX: Recreate compute descriptor sets during swapchain recreation
    // This is essential because compute descriptor sets can become stale during swapchain recreation
    
    // Validate that we have the compute descriptor set layout
    if (computeDescriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "GPUEntityManager: ERROR - Cannot recreate compute descriptor sets: layout not available" << std::endl;
        return false;
    }
    
    // Validate buffers are available
    if (entityBuffer == VK_NULL_HANDLE || positionBuffer == VK_NULL_HANDLE || 
        currentPositionBuffer == VK_NULL_HANDLE || targetPositionBuffer == VK_NULL_HANDLE) {
        std::cerr << "GPUEntityManager: ERROR - Cannot recreate compute descriptor sets: buffers not available" << std::endl;
        return false;
    }
    
    // CRITICAL FIX: Reset the descriptor pool to allow reallocation
    // The pool was created with maxSets=1, so we need to reset it to reallocate
    if (computeDescriptorPool != VK_NULL_HANDLE) {
        if (context->getLoader().vkResetDescriptorPool(context->getDevice(), computeDescriptorPool, 0) != VK_SUCCESS) {
            std::cerr << "GPUEntityManager: ERROR - Failed to reset compute descriptor pool" << std::endl;
            return false;
        }
        std::cout << "GPUEntityManager: Reset compute descriptor pool for reallocation" << std::endl;
        computeDescriptorSet = VK_NULL_HANDLE; // Pool reset invalidates all descriptor sets
    }
    
    // Allocate new descriptor set (reusing same layout and pool)
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computeDescriptorSetLayout;

    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
        std::cerr << "GPUEntityManager: ERROR - Failed to reallocate compute descriptor set" << std::endl;
        return false;
    }

    // Update descriptor set with current buffer bindings (same as createComputeDescriptorSets)
    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    // Binding 0: Entity buffer
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = entityBuffer;
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
    positionBufferInfo.buffer = positionBuffer;
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
    currentPosBufferInfo.buffer = currentPositionBuffer;
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
    targetPosBufferInfo.buffer = targetPositionBuffer;
    targetPosBufferInfo.offset = 0;
    targetPosBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = computeDescriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &targetPosBufferInfo;

    // Update all descriptor bindings
    context->getLoader().vkUpdateDescriptorSets(context->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    std::cout << "GPUEntityManager: Compute descriptor sets successfully recreated during swapchain recreation" << std::endl;
    return true;
}