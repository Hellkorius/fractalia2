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

bool VulkanResources::initialize(VulkanContext* context, VulkanSync* sync, VulkanFunctionLoader* loader) {
    this->context = context;
    this->sync = sync;
    this->loader = loader;
    
    if (!loader) {
        std::cerr << "VulkanResources requires VulkanFunctionLoader" << std::endl;
        return false;
    }
    
    return true;
}

void VulkanResources::cleanup() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (i < uniformBuffers.size() && uniformBuffers[i] != VK_NULL_HANDLE) {
            loader->vkDestroyBuffer(context->getDevice(), uniformBuffers[i], nullptr);
        }
        if (i < uniformBuffersMemory.size() && uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            loader->vkFreeMemory(context->getDevice(), uniformBuffersMemory[i], nullptr);
        }
    }
    
    // Clean up instance buffer
    if (instanceBuffer != VK_NULL_HANDLE) {
        loader->vkDestroyBuffer(context->getDevice(), instanceBuffer, nullptr);
        instanceBuffer = VK_NULL_HANDLE;
    }
    if (instanceBufferMemory != VK_NULL_HANDLE) {
        loader->vkFreeMemory(context->getDevice(), instanceBufferMemory, nullptr);
        instanceBufferMemory = VK_NULL_HANDLE;
    }
    
    if (vertexBuffer != VK_NULL_HANDLE) {
        loader->vkDestroyBuffer(context->getDevice(), vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        loader->vkFreeMemory(context->getDevice(), vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    
    if (indexBuffer != VK_NULL_HANDLE) {
        loader->vkDestroyBuffer(context->getDevice(), indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        loader->vkFreeMemory(context->getDevice(), indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    
    
    if (descriptorPool != VK_NULL_HANDLE) {
        loader->vkDestroyDescriptorPool(context->getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

bool VulkanResources::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 2;
    
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VulkanUtils::createBuffer(context->getDevice(), *loader, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers[i], uniformBuffersMemory[i]);
        
        loader->vkMapMemory(context->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
    
    return true;
}

bool VulkanResources::createVertexBuffer() {
    return createTriangleBuffers();
}

bool VulkanResources::createIndexBuffer() {
    // Index buffer is now created as part of createTriangleBuffers()
    return true;
}

bool VulkanResources::createInstanceBuffer() {
    VulkanUtils::createBuffer(context->getDevice(), *loader, INSTANCE_BUFFER_SIZE, 
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceBufferMemory);
    
    loader->vkMapMemory(context->getDevice(), instanceBufferMemory, 0, INSTANCE_BUFFER_SIZE, 0, &instanceBufferMapped);
    
    return true;
}


bool VulkanResources::createTriangleBuffers() {
    PolygonMesh triangle = PolygonFactory::createTriangle();
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * triangle.vertices.size();
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanUtils::createBuffer(context->getDevice(), *loader, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    void* data;
    loader->vkMapMemory(context->getDevice(), stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, triangle.vertices.data(), (size_t)vertexBufferSize);
    loader->vkUnmapMemory(context->getDevice(), stagingBufferMemory);
    
    VulkanUtils::createBuffer(context->getDevice(), *loader, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    
    VulkanUtils::copyBuffer(context->getDevice(), *loader, context->getGraphicsQueue(), sync->getCommandPool(), stagingBuffer, vertexBuffer, vertexBufferSize);
    
    loader->vkDestroyBuffer(context->getDevice(), stagingBuffer, nullptr);
    loader->vkFreeMemory(context->getDevice(), stagingBufferMemory, nullptr);
    
    // Create index buffer
    indexCount = static_cast<uint32_t>(triangle.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * triangle.indices.size();
    
    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    VulkanUtils::createBuffer(context->getDevice(), *loader, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexStagingBuffer, indexStagingBufferMemory);
    
    loader->vkMapMemory(context->getDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, triangle.indices.data(), (size_t)indexBufferSize);
    loader->vkUnmapMemory(context->getDevice(), indexStagingBufferMemory);
    
    VulkanUtils::createBuffer(context->getDevice(), *loader, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    
    VulkanUtils::copyBuffer(context->getDevice(), *loader, context->getGraphicsQueue(), sync->getCommandPool(), indexStagingBuffer, indexBuffer, indexBufferSize);
    
    loader->vkDestroyBuffer(context->getDevice(), indexStagingBuffer, nullptr);
    loader->vkFreeMemory(context->getDevice(), indexStagingBufferMemory, nullptr);
    
    return true;
}


bool VulkanResources::createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    const uint32_t maxSets = 1024;
    
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    
    // Uniform buffers
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxSets;
    
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    
    if (loader->vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
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
    if (loader->vkAllocateDescriptorSets(context->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets!" << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
        
        // UBO binding
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(glm::mat4) * 2;
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        
        loader->vkUpdateDescriptorSets(context->getDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
    
    return true;
}


uint32_t VulkanResources::findMemoryType(VulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    context->vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanResources::createImage(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits numSamples,
                                 VkImage& image, VkDeviceMemory& imageMemory) {
    throw std::runtime_error("VulkanResources::createImage is deprecated. Use VulkanUtils::createImage instead.");
}

VkImageView VulkanResources::createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    throw std::runtime_error("VulkanResources::createImageView is deprecated. Use VulkanUtils::createImageView instead.");
}

void VulkanResources::createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    throw std::runtime_error("VulkanResources::createBuffer is deprecated. Use VulkanUtils::createBuffer instead.");
}

void VulkanResources::copyBuffer(VulkanContext* context, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    throw std::runtime_error("VulkanResources::copyBuffer is deprecated. Use VulkanUtils::copyBuffer instead.");
}

