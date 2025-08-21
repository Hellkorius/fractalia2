#include "graphics_resource_manager.h"
#include "buffer_factory.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_constants.h"
#include "../core/vulkan_utils.h"
#include "../../PolygonFactory.h"
#include <iostream>
#include <glm/glm.hpp>

GraphicsResourceManager::GraphicsResourceManager() {
}

GraphicsResourceManager::~GraphicsResourceManager() {
    cleanup();
}

bool GraphicsResourceManager::initialize(const VulkanContext& context, BufferFactory* bufferFactory) {
    this->context = &context;
    this->bufferFactory = bufferFactory;
    return true;
}

void GraphicsResourceManager::cleanup() {
    cleanupBeforeContextDestruction();
    context = nullptr;
    bufferFactory = nullptr;
    resourcesNeedRecreation = false;
}

void GraphicsResourceManager::cleanupBeforeContextDestruction() {
    // Clean up graphics resources first
    for (auto& handle : uniformBufferHandles) {
        if (handle.isValid()) {
            bufferFactory->destroyResource(handle);
        }
    }
    uniformBufferHandles.clear();
    uniformBuffers.clear();
    uniformBuffersMapped.clear();
    
    if (vertexBufferHandle.isValid()) {
        bufferFactory->destroyResource(vertexBufferHandle);
    }
    if (indexBufferHandle.isValid()) {
        bufferFactory->destroyResource(indexBufferHandle);
    }
    
    // RAII wrapper will handle cleanup automatically
    graphicsDescriptorPool.reset();
    markForRecreation();
}

bool GraphicsResourceManager::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 2;
    
    uniformBufferHandles.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformBufferHandles[i] = bufferFactory->createMappedBuffer(
            bufferSize, 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        if (!uniformBufferHandles[i].isValid()) {
            std::cerr << "Failed to create uniform buffer " << i << std::endl;
            return false;
        }
        
        // Maintain compatibility with existing API
        uniformBuffers[i] = uniformBufferHandles[i].buffer.get();
        uniformBuffersMapped[i] = uniformBufferHandles[i].mappedData;
    }
    
    return true;
}

bool GraphicsResourceManager::createTriangleBuffers() {
    PolygonMesh triangle = PolygonFactory::createTriangle();
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * triangle.vertices.size();
    
    // Create staging buffer
    ResourceHandle stagingHandle = bufferFactory->createMappedBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    if (!stagingHandle.isValid()) {
        std::cerr << "Failed to create vertex staging buffer!" << std::endl;
        return false;
    }
    
    // Copy vertex data to staging buffer
    memcpy(stagingHandle.mappedData, triangle.vertices.data(), (size_t)vertexBufferSize);
    
    // Create device-local vertex buffer
    vertexBufferHandle = bufferFactory->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    if (!vertexBufferHandle.isValid()) {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        bufferFactory->destroyResource(stagingHandle);
        return false;
    }
    
    // Copy from staging to device buffer
    bufferFactory->copyBufferToBuffer(stagingHandle, vertexBufferHandle, vertexBufferSize);
    
    // Clean up staging buffer
    bufferFactory->destroyResource(stagingHandle);
    
    // Create index buffer
    indexCount = static_cast<uint32_t>(triangle.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * triangle.indices.size();
    
    // Create index staging buffer
    ResourceHandle indexStagingHandle = bufferFactory->createMappedBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    if (!indexStagingHandle.isValid()) {
        std::cerr << "Failed to create index staging buffer!" << std::endl;
        return false;
    }
    
    // Copy index data to staging buffer
    memcpy(indexStagingHandle.mappedData, triangle.indices.data(), (size_t)indexBufferSize);
    
    // Create device-local index buffer
    indexBufferHandle = bufferFactory->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    if (!indexBufferHandle.isValid()) {
        std::cerr << "Failed to create index buffer!" << std::endl;
        bufferFactory->destroyResource(indexStagingHandle);
        return false;
    }
    
    // Copy from staging to device buffer
    bufferFactory->copyBufferToBuffer(indexStagingHandle, indexBufferHandle, indexBufferSize);
    
    // Clean up staging buffer
    bufferFactory->destroyResource(indexStagingHandle);
    
    return true;
}

bool GraphicsResourceManager::createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DEFAULT_MAX_DESCRIPTOR_SETS},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DEFAULT_MAX_DESCRIPTOR_SETS}
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = DEFAULT_MAX_DESCRIPTOR_SETS;
    
    graphicsDescriptorPool = vulkan_raii::create_descriptor_pool(context, &poolInfo);
    
    if (!graphicsDescriptorPool) {
        std::cerr << "Failed to create graphics descriptor pool!" << std::endl;
        return false;
    }
    
    return true;
}

bool GraphicsResourceManager::createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    // Cache the layout for potential recreation
    cachedDescriptorLayout = descriptorSetLayout;
    
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool.get();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    graphicsDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, graphicsDescriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate graphics descriptor sets!" << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Use helper for uniform buffer (binding 0)
        std::vector<VkDescriptorBufferInfo> bufferInfos = {bufferInfo};
        VulkanUtils::writeDescriptorSets(context->getDevice(), context->getLoader(), graphicsDescriptorSets[i], bufferInfos, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }
    
    return true;
}

bool GraphicsResourceManager::recreateGraphicsDescriptors() {
    if (!context) {
        std::cerr << "GraphicsResourceManager: Cannot recreate descriptors - no context" << std::endl;
        return false;
    }
    
    if (cachedDescriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: Cannot recreate descriptors - no cached layout" << std::endl;
        std::cerr << "  This indicates the GraphicsResourceManager was reinitialized and lost its cached state." << std::endl;
        std::cerr << "  The descriptor layout should be preserved across swapchain recreation." << std::endl;
        return false;
    }
    
    std::cout << "GraphicsResourceManager: Recreating graphics descriptors after swapchain recreation" << std::endl;
    
    // Recreate descriptor pool if needed
    if (!graphicsDescriptorPool) {
        if (!createGraphicsDescriptorPool(cachedDescriptorLayout)) {
            std::cerr << "GraphicsResourceManager: Failed to recreate descriptor pool" << std::endl;
            return false;
        }
    }
    
    // Recreate descriptor sets
    if (!createGraphicsDescriptorSets(cachedDescriptorLayout)) {
        std::cerr << "GraphicsResourceManager: Failed to recreate descriptor sets" << std::endl;
        return false;
    }
    
    std::cout << "GraphicsResourceManager: Successfully recreated graphics descriptors" << std::endl;
    return true;
}

bool GraphicsResourceManager::updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding (binding 0)
        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = uniformBuffers[i];
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Position buffer binding (binding 2)
        VkDescriptorBufferInfo positionBufferInfo{};
        positionBufferInfo.buffer = positionBuffer;
        positionBufferInfo.offset = 0;
        positionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Write both descriptors
        VkWriteDescriptorSet descriptorWrites[2] = {};
        
        // UBO write
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Position buffer write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &positionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

bool GraphicsResourceManager::updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding (binding 0)
        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = uniformBuffers[i];
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Current position buffer binding (binding 2)
        VkDescriptorBufferInfo currentPositionBufferInfo{};
        currentPositionBufferInfo.buffer = currentPositionBuffer;
        currentPositionBufferInfo.offset = 0;
        currentPositionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Target position buffer binding (binding 3)
        VkDescriptorBufferInfo targetPositionBufferInfo{};
        targetPositionBufferInfo.buffer = targetPositionBuffer;
        targetPositionBufferInfo.offset = 0;
        targetPositionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Write all three descriptors
        VkWriteDescriptorSet descriptorWrites[3] = {};
        
        // UBO write
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Current position buffer write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &currentPositionBufferInfo;
        
        // Target position buffer write
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[2].dstBinding = 3;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &targetPositionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 3, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

bool GraphicsResourceManager::updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    // Validate inputs
    if (entityBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Entity buffer is null in updateDescriptorSetsWithEntityAndPositionBuffers" << std::endl;
        return false;
    }
    if (positionBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Position buffer is null in updateDescriptorSetsWithEntityAndPositionBuffers" << std::endl;
        return false;
    }
    // Auto-recreate descriptor sets if they were destroyed (e.g., during resize)
    if (graphicsDescriptorSets.empty()) {
        std::cout << "GraphicsResourceManager: Descriptor sets missing, attempting to recreate..." << std::endl;
        if (recreateGraphicsDescriptors()) {
            std::cout << "GraphicsResourceManager: Successfully recreated graphics descriptors" << std::endl;
        } else {
            std::cerr << "GraphicsResourceManager: CRITICAL ERROR - Failed to recreate graphics descriptors!" << std::endl;
            return false;
        }
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding (binding 0)
        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = uniformBuffers[i];
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Entity buffer binding (binding 1)
        VkDescriptorBufferInfo entityBufferInfo{};
        entityBufferInfo.buffer = entityBuffer;
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = VK_WHOLE_SIZE;
        
        // Position buffer binding (binding 2)
        VkDescriptorBufferInfo positionBufferInfo{};
        positionBufferInfo.buffer = positionBuffer;
        positionBufferInfo.offset = 0;
        positionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Write all three descriptors
        VkWriteDescriptorSet descriptorWrites[3] = {};
        
        // UBO write (binding 0)
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Entity buffer write (binding 1)
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &entityBufferInfo;
        
        // Position buffer write (binding 2)
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &positionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 3, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

// High-level resource operations (consolidated from facade)
bool GraphicsResourceManager::createAllGraphicsResources() {
    if (!context || !bufferFactory) {
        return false;
    }
    
    // Create resources in proper order
    bool success = true;
    
    success &= createUniformBuffers();
    success &= createTriangleBuffers();
    
    if (success) {
        clearRecreationFlag();
    }
    
    return success;
}

bool GraphicsResourceManager::recreateGraphicsResources() {
    bool success = recreateGraphicsDescriptors();
    
    if (success) {
        clearRecreationFlag();
    }
    
    return success;
}

// Resource state queries (from facade)
bool GraphicsResourceManager::areResourcesCreated() const {
    return !uniformBufferHandles.empty() && 
           vertexBufferHandle.isValid() && 
           indexBufferHandle.isValid();
}

bool GraphicsResourceManager::areDescriptorsCreated() const {
    return graphicsDescriptorPool && !graphicsDescriptorSets.empty();
}

// Memory optimization (from facade)
bool GraphicsResourceManager::optimizeGraphicsMemoryUsage() {
    // Basic optimization - could be expanded
    return bufferFactory != nullptr;
}

VkDeviceSize GraphicsResourceManager::getGraphicsMemoryFootprint() const {
    VkDeviceSize total = 0;
    
    // Add uniform buffer sizes
    for (const auto& handle : uniformBufferHandles) {
        if (handle.isValid()) {
            total += sizeof(glm::mat4) * 2; // Known size
        }
    }
    
    // Add vertex/index buffer sizes (approximation)
    if (vertexBufferHandle.isValid()) {
        total += sizeof(Vertex) * 3; // Triangle vertices
    }
    if (indexBufferHandle.isValid()) {
        total += sizeof(uint16_t) * 3; // Triangle indices  
    }
    
    return total;
}