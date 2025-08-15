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

bool VulkanResources::initialize(const VulkanContext& context, VulkanSync* sync) {
    this->context = &context;
    this->sync = sync;
    
    return true;
}

void VulkanResources::cleanup() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (i < uniformBuffers.size() && uniformBuffers[i] != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyBuffer(context->getDevice(), uniformBuffers[i], nullptr);
        }
        if (i < uniformBuffersMemory.size() && uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            context->getLoader().vkFreeMemory(context->getDevice(), uniformBuffersMemory[i], nullptr);
        }
    }
    
    
    if (vertexBuffer != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyBuffer(context->getDevice(), vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        context->getLoader().vkFreeMemory(context->getDevice(), vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    
    if (indexBuffer != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyBuffer(context->getDevice(), indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        context->getLoader().vkFreeMemory(context->getDevice(), indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    
    
    if (descriptorPool != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDescriptorPool(context->getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

bool VulkanResources::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 2;
    
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VulkanUtils::createBuffer(context->getDevice(), context->getLoader(), bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers[i], uniformBuffersMemory[i]);
        
        context->getLoader().vkMapMemory(context->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
    
    return true;
}

bool VulkanResources::createTriangleBuffers() {
    PolygonMesh triangle = PolygonFactory::createTriangle();
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * triangle.vertices.size();
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanUtils::createBuffer(context->getDevice(), context->getLoader(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    void* data;
    context->getLoader().vkMapMemory(context->getDevice(), stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, triangle.vertices.data(), (size_t)vertexBufferSize);
    context->getLoader().vkUnmapMemory(context->getDevice(), stagingBufferMemory);
    
    VulkanUtils::createBuffer(context->getDevice(), context->getLoader(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    
    VulkanUtils::copyBuffer(context->getDevice(), context->getLoader(), context->getGraphicsQueue(), sync->getCommandPool(), stagingBuffer, vertexBuffer, vertexBufferSize);
    
    context->getLoader().vkDestroyBuffer(context->getDevice(), stagingBuffer, nullptr);
    context->getLoader().vkFreeMemory(context->getDevice(), stagingBufferMemory, nullptr);
    
    // Create index buffer
    indexCount = static_cast<uint32_t>(triangle.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * triangle.indices.size();
    
    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    VulkanUtils::createBuffer(context->getDevice(), context->getLoader(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexStagingBuffer, indexStagingBufferMemory);
    
    context->getLoader().vkMapMemory(context->getDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, triangle.indices.data(), (size_t)indexBufferSize);
    context->getLoader().vkUnmapMemory(context->getDevice(), indexStagingBufferMemory);
    
    VulkanUtils::createBuffer(context->getDevice(), context->getLoader(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    
    VulkanUtils::copyBuffer(context->getDevice(), context->getLoader(), context->getGraphicsQueue(), sync->getCommandPool(), indexStagingBuffer, indexBuffer, indexBufferSize);
    
    context->getLoader().vkDestroyBuffer(context->getDevice(), indexStagingBuffer, nullptr);
    context->getLoader().vkFreeMemory(context->getDevice(), indexStagingBufferMemory, nullptr);
    
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
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    
    if (context->getLoader().vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
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




