#include "vulkan_resources.h"
#include "vulkan_sync.h"
#include "vulkan_function_loader.h"
#include "vulkan_utils.h"
#include <iostream>
#include <stdexcept>
#include <array>
#include <glm/glm.hpp>

VulkanResources::VulkanResources() {
}

VulkanResources::~VulkanResources() {
    cleanup();
}

bool VulkanResources::initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext) {
    this->context = &context;
    this->sync = sync;
    this->resourceContext = resourceContext;
    
    return true;
}

void VulkanResources::cleanup() {
    // Clean up uniform buffers using ResourceContext
    for (auto& handle : uniformBufferHandles) {
        if (resourceContext) {
            resourceContext->destroyResource(handle);
        }
    }
    uniformBufferHandles.clear();
    uniformBuffers.clear();
    uniformBuffersMapped.clear();
    
    // Clean up vertex and index buffers
    if (resourceContext) {
        if (vertexBufferHandle.isValid()) {
            resourceContext->destroyResource(vertexBufferHandle);
        }
        if (indexBufferHandle.isValid()) {
            resourceContext->destroyResource(indexBufferHandle);
        }
    }
    
    // Clean up descriptor pool
    if (descriptorPool != VK_NULL_HANDLE) {
        if (resourceContext) {
            resourceContext->destroyDescriptorPool(descriptorPool);
        }
        descriptorPool = VK_NULL_HANDLE;
    }
}

bool VulkanResources::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 2;
    
    uniformBufferHandles.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformBufferHandles[i] = resourceContext->createMappedBuffer(
            bufferSize, 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        if (!uniformBufferHandles[i].isValid()) {
            std::cerr << "Failed to create uniform buffer " << i << std::endl;
            return false;
        }
        
        // Maintain compatibility with existing API
        uniformBuffers[i] = uniformBufferHandles[i].buffer;
        uniformBuffersMapped[i] = uniformBufferHandles[i].mappedData;
    }
    
    return true;
}

bool VulkanResources::createTriangleBuffers() {
    PolygonMesh triangle = PolygonFactory::createTriangle();
    
    // Create vertex buffer using ResourceContext
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * triangle.vertices.size();
    
    // Create staging buffer using ResourceContext
    ResourceHandle stagingHandle = resourceContext->createMappedBuffer(
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
    vertexBufferHandle = resourceContext->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    if (!vertexBufferHandle.isValid()) {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        resourceContext->destroyResource(stagingHandle);
        return false;
    }
    
    // Copy from staging to device buffer (simplified - would use command buffers)
    VulkanUtils::copyBuffer(context->getDevice(), context->getLoader(), context->getGraphicsQueue(), 
                          sync->getCommandPool(), stagingHandle.buffer, vertexBufferHandle.buffer, vertexBufferSize);
    
    // Clean up staging buffer
    resourceContext->destroyResource(stagingHandle);
    
    // Create index buffer using ResourceContext
    indexCount = static_cast<uint32_t>(triangle.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * triangle.indices.size();
    
    // Create index staging buffer
    ResourceHandle indexStagingHandle = resourceContext->createMappedBuffer(
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
    indexBufferHandle = resourceContext->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    if (!indexBufferHandle.isValid()) {
        std::cerr << "Failed to create index buffer!" << std::endl;
        resourceContext->destroyResource(indexStagingHandle);
        return false;
    }
    
    // Copy from staging to device buffer
    VulkanUtils::copyBuffer(context->getDevice(), context->getLoader(), context->getGraphicsQueue(), 
                          sync->getCommandPool(), indexStagingHandle.buffer, indexBufferHandle.buffer, indexBufferSize);
    
    // Clean up staging buffer
    resourceContext->destroyResource(indexStagingHandle);
    
    return true;
}


bool VulkanResources::createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    // Use ResourceContext for descriptor pool creation
    ResourceContext::DescriptorPoolConfig config;
    config.maxSets = 1024;
    config.uniformBuffers = 1024;
    config.storageBuffers = 0;
    config.sampledImages = 0;
    config.allowFreeDescriptorSets = true;
    
    descriptorPool = resourceContext->createDescriptorPool(config);
    
    if (descriptorPool == VK_NULL_HANDLE) {
        std::cerr << "Failed to create descriptor pool!" << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanResources::createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets!" << std::endl;
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
        VulkanUtils::writeDescriptorSets(context->getDevice(), context->getLoader(), descriptorSets[i], bufferInfos, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }
    
    return true;
}

bool VulkanResources::updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer) {
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
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Position buffer write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &positionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

bool VulkanResources::updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer) {
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
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Current position buffer write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &currentPositionBufferInfo;
        
        // Target position buffer write
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 3;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &targetPositionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 3, descriptorWrites, 0, nullptr);
    }
    
    return true;
}
