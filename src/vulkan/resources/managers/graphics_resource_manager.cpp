#include "graphics_resource_manager.h"
#include "../buffers/buffer_factory.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include "../../core/vulkan_constants.h"
#include "../../core/vulkan_utils.h"
#include "../descriptors/descriptor_update_helper.h"
#include "../../../PolygonFactory.h"
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

bool GraphicsResourceManager::createGeometryBuffers() {
    PolygonMesh cube = PolygonFactory::createCube();
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * cube.vertices.size();
    
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
    memcpy(stagingHandle.mappedData, cube.vertices.data(), (size_t)vertexBufferSize);
    
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
    indexCount = static_cast<uint32_t>(cube.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * cube.indices.size();
    
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
    memcpy(indexStagingHandle.mappedData, cube.indices.data(), (size_t)indexBufferSize);
    
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

bool GraphicsResourceManager::updateDescriptorSets(const std::vector<DescriptorUpdateHelper::BufferBinding>& additionalBindings) {
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

    // Build base bindings (uniform buffer binding 0)
    std::vector<DescriptorUpdateHelper::BufferBinding> bindings;
    
    // Add additional bindings
    bindings.insert(bindings.end(), additionalBindings.begin(), additionalBindings.end());
    
    // Update each frame's descriptor set
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSetArray;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        descriptorSetArray[i] = graphicsDescriptorSets[i];
    }
    
    // Use specialized uniform buffer update helper
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBufferArray;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        uniformBufferArray[i] = uniformBuffers[i];
    }
    
    // Update uniform buffer binding (binding 0) for all frames
    if (!DescriptorUpdateHelper::updateUniformBufferBinding(
            *context, descriptorSetArray, 0, uniformBufferArray, sizeof(glm::mat4) * 2)) {
        std::cerr << "GraphicsResourceManager: Failed to update uniform buffer binding" << std::endl;
        return false;
    }
    
    // Update additional bindings if provided
    if (!additionalBindings.empty()) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (!DescriptorUpdateHelper::updateDescriptorSet(*context, graphicsDescriptorSets[i], additionalBindings)) {
                std::cerr << "GraphicsResourceManager: Failed to update additional bindings for frame " << i << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

bool GraphicsResourceManager::updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer) {
    // Validate inputs
    if (positionBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Position buffer is null" << std::endl;
        return false;
    }
    
    std::vector<DescriptorUpdateHelper::BufferBinding> bindings = {
        {2, positionBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}  // Position buffer at binding 2
    };
    
    return updateDescriptorSets(bindings);
}

bool GraphicsResourceManager::updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer) {
    // Validate inputs
    if (currentPositionBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Current position buffer is null" << std::endl;
        return false;
    }
    if (targetPositionBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Target position buffer is null" << std::endl;
        return false;
    }
    
    std::vector<DescriptorUpdateHelper::BufferBinding> bindings = {
        {2, currentPositionBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},  // Current position at binding 2
        {3, targetPositionBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}    // Target position at binding 3
    };
    
    return updateDescriptorSets(bindings);
}

bool GraphicsResourceManager::updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    // Validate inputs
    if (entityBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Entity buffer is null" << std::endl;
        return false;
    }
    if (positionBuffer == VK_NULL_HANDLE) {
        std::cerr << "GraphicsResourceManager: ERROR - Position buffer is null" << std::endl;
        return false;
    }
    
    std::vector<DescriptorUpdateHelper::BufferBinding> bindings = {
        {1, entityBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},   // Entity buffer at binding 1
        {2, positionBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}  // Position buffer at binding 2
    };
    
    return updateDescriptorSets(bindings);
}

// High-level resource operations (consolidated from facade)
bool GraphicsResourceManager::createAllGraphicsResources() {
    if (!context || !bufferFactory) {
        return false;
    }
    
    // Create resources in proper order
    bool success = true;
    
    success &= createUniformBuffers();
    success &= createGeometryBuffers();
    
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
        total += sizeof(Vertex) * 8; // Cube vertices
    }
    if (indexBufferHandle.isValid()) {
        total += sizeof(uint16_t) * 36; // Cube indices (12 triangles * 3 indices)  
    }
    
    return total;
}