#include "vulkan_resources.h"
#include "vulkan_sync.h"
#include <iostream>
#include <stdexcept>
#include <array>
#include <glm/glm.hpp>

VulkanResources::VulkanResources() {
}

VulkanResources::~VulkanResources() {
    cleanup();
}

bool VulkanResources::initialize(VulkanContext* context, VulkanSync* sync) {
    this->context = context;
    this->sync = sync;
    
    loadFunctions();
    
    return true;
}

void VulkanResources::cleanup() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (i < uniformBuffers.size() && uniformBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(context->getDevice(), uniformBuffers[i], nullptr);
        }
        if (i < uniformBuffersMemory.size() && uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(context->getDevice(), uniformBuffersMemory[i], nullptr);
        }
    }
    
    for (size_t i = 0; i < instanceBuffers.size(); i++) {
        if (instanceBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(context->getDevice(), instanceBuffers[i], nullptr);
        }
        if (i < instanceBuffersMemory.size() && instanceBuffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(context->getDevice(), instanceBuffersMemory[i], nullptr);
        }
    }
    
    // Clean up triangle buffers
    if (triangleVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), triangleVertexBuffer, nullptr);
        triangleVertexBuffer = VK_NULL_HANDLE;
    }
    if (triangleVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), triangleVertexBufferMemory, nullptr);
        triangleVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (triangleIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), triangleIndexBuffer, nullptr);
        triangleIndexBuffer = VK_NULL_HANDLE;
    }
    if (triangleIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), triangleIndexBufferMemory, nullptr);
        triangleIndexBufferMemory = VK_NULL_HANDLE;
    }
    
    // Clean up square buffers
    if (squareVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), squareVertexBuffer, nullptr);
        squareVertexBuffer = VK_NULL_HANDLE;
    }
    if (squareVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), squareVertexBufferMemory, nullptr);
        squareVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (squareIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), squareIndexBuffer, nullptr);
        squareIndexBuffer = VK_NULL_HANDLE;
    }
    if (squareIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), squareIndexBufferMemory, nullptr);
        squareIndexBufferMemory = VK_NULL_HANDLE;
    }
    
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    
    // Clean up keyframe buffer
    if (keyframeBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), keyframeBuffer, nullptr);
        keyframeBuffer = VK_NULL_HANDLE;
    }
    if (keyframeBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), keyframeBufferMemory, nullptr);
        keyframeBufferMemory = VK_NULL_HANDLE;
    }
    
    // Clean up keyframe descriptor sets
    if (keyframeDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context->getDevice(), keyframeDescriptorPool, nullptr);
        keyframeDescriptorPool = VK_NULL_HANDLE;
    }
    if (keyframeDescriptorSetLayout != VK_NULL_HANDLE && this->vkDestroyDescriptorSetLayout) {
        this->vkDestroyDescriptorSetLayout(context->getDevice(), keyframeDescriptorSetLayout, nullptr);
        keyframeDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context->getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

bool VulkanResources::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 2;
    
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(context, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers[i], uniformBuffersMemory[i]);
        
        vkMapMemory(context->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
    
    return true;
}

bool VulkanResources::createVertexBuffer() {
    return createMultiShapeBuffers();
}

bool VulkanResources::createIndexBuffer() {
    // Index buffer is now created as part of createPolygonBuffers()
    // This method is kept for compatibility but does nothing
    return true;
}

bool VulkanResources::createInstanceBuffers() {
    instanceBuffers.resize(STAGING_BUFFER_COUNT);
    instanceBuffersMemory.resize(STAGING_BUFFER_COUNT);
    instanceBuffersMapped.resize(STAGING_BUFFER_COUNT);
    
    for (int i = 0; i < STAGING_BUFFER_COUNT; i++) {
        createBuffer(context, STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     instanceBuffers[i], instanceBuffersMemory[i]);
        
        vkMapMemory(context->getDevice(), instanceBuffersMemory[i], 0, STAGING_BUFFER_SIZE, 0, &instanceBuffersMapped[i]);
    }
    
    return true;
}

bool VulkanResources::createKeyframeBuffers() {
    VkDeviceSize bufferSize = MAX_KEYFRAMES * KEYFRAME_SIZE; // 100 frames × max entities × keyframe data
    
    createBuffer(context, bufferSize, 
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 keyframeBuffer, keyframeBufferMemory);
    
    vkMapMemory(context->getDevice(), keyframeBufferMemory, 0, bufferSize, 0, &keyframeBufferMapped);
    
    return true;
}

bool VulkanResources::createMultiShapeBuffers() {
    // Create buffers for triangle
    PolygonMesh triangle = PolygonFactory::createTriangle();
    
    VkDeviceSize triangleVertexBufferSize = sizeof(Vertex) * triangle.vertices.size();
    
    VkBuffer triangleStagingBuffer;
    VkDeviceMemory triangleStagingBufferMemory;
    createBuffer(context, triangleVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 triangleStagingBuffer, triangleStagingBufferMemory);
    
    void* data;
    vkMapMemory(context->getDevice(), triangleStagingBufferMemory, 0, triangleVertexBufferSize, 0, &data);
    memcpy(data, triangle.vertices.data(), (size_t)triangleVertexBufferSize);
    vkUnmapMemory(context->getDevice(), triangleStagingBufferMemory);
    
    createBuffer(context, triangleVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, triangleVertexBuffer, triangleVertexBufferMemory);
    
    copyBuffer(context, sync->getCommandPool(), triangleStagingBuffer, triangleVertexBuffer, triangleVertexBufferSize);
    
    vkDestroyBuffer(context->getDevice(), triangleStagingBuffer, nullptr);
    vkFreeMemory(context->getDevice(), triangleStagingBufferMemory, nullptr);
    
    // Create triangle index buffer
    triangleIndexCount = static_cast<uint32_t>(triangle.indices.size());
    VkDeviceSize triangleIndexBufferSize = sizeof(uint16_t) * triangle.indices.size();
    
    VkBuffer triangleIndexStagingBuffer;
    VkDeviceMemory triangleIndexStagingBufferMemory;
    createBuffer(context, triangleIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 triangleIndexStagingBuffer, triangleIndexStagingBufferMemory);
    
    vkMapMemory(context->getDevice(), triangleIndexStagingBufferMemory, 0, triangleIndexBufferSize, 0, &data);
    memcpy(data, triangle.indices.data(), (size_t)triangleIndexBufferSize);
    vkUnmapMemory(context->getDevice(), triangleIndexStagingBufferMemory);
    
    createBuffer(context, triangleIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, triangleIndexBuffer, triangleIndexBufferMemory);
    
    copyBuffer(context, sync->getCommandPool(), triangleIndexStagingBuffer, triangleIndexBuffer, triangleIndexBufferSize);
    
    vkDestroyBuffer(context->getDevice(), triangleIndexStagingBuffer, nullptr);
    vkFreeMemory(context->getDevice(), triangleIndexStagingBufferMemory, nullptr);
    
    // Create buffers for square
    PolygonMesh square = PolygonFactory::createSquare();
    
    VkDeviceSize squareVertexBufferSize = sizeof(Vertex) * square.vertices.size();
    
    VkBuffer squareStagingBuffer;
    VkDeviceMemory squareStagingBufferMemory;
    createBuffer(context, squareVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 squareStagingBuffer, squareStagingBufferMemory);
    
    vkMapMemory(context->getDevice(), squareStagingBufferMemory, 0, squareVertexBufferSize, 0, &data);
    memcpy(data, square.vertices.data(), (size_t)squareVertexBufferSize);
    vkUnmapMemory(context->getDevice(), squareStagingBufferMemory);
    
    createBuffer(context, squareVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, squareVertexBuffer, squareVertexBufferMemory);
    
    copyBuffer(context, sync->getCommandPool(), squareStagingBuffer, squareVertexBuffer, squareVertexBufferSize);
    
    vkDestroyBuffer(context->getDevice(), squareStagingBuffer, nullptr);
    vkFreeMemory(context->getDevice(), squareStagingBufferMemory, nullptr);
    
    // Create square index buffer
    squareIndexCount = static_cast<uint32_t>(square.indices.size());
    VkDeviceSize squareIndexBufferSize = sizeof(uint16_t) * square.indices.size();
    
    VkBuffer squareIndexStagingBuffer;
    VkDeviceMemory squareIndexStagingBufferMemory;
    createBuffer(context, squareIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 squareIndexStagingBuffer, squareIndexStagingBufferMemory);
    
    vkMapMemory(context->getDevice(), squareIndexStagingBufferMemory, 0, squareIndexBufferSize, 0, &data);
    memcpy(data, square.indices.data(), (size_t)squareIndexBufferSize);
    vkUnmapMemory(context->getDevice(), squareIndexStagingBufferMemory);
    
    createBuffer(context, squareIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, squareIndexBuffer, squareIndexBufferMemory);
    
    copyBuffer(context, sync->getCommandPool(), squareIndexStagingBuffer, squareIndexBuffer, squareIndexBufferSize);
    
    vkDestroyBuffer(context->getDevice(), squareIndexStagingBuffer, nullptr);
    vkFreeMemory(context->getDevice(), squareIndexStagingBufferMemory, nullptr);
    
    
    return true;
}

bool VulkanResources::createPolygonBuffers(const PolygonMesh& polygon) {
    // Create vertex buffer from polygon data
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * polygon.vertices.size();
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(context, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(context->getDevice(), stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, polygon.vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(context->getDevice(), stagingBufferMemory);
    
    createBuffer(context, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    
    copyBuffer(context, sync->getCommandPool(), stagingBuffer, vertexBuffer, vertexBufferSize);
    
    vkDestroyBuffer(context->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(context->getDevice(), stagingBufferMemory, nullptr);
    
    // Create index buffer from polygon data
    indexCount = static_cast<uint32_t>(polygon.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * polygon.indices.size();
    
    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    createBuffer(context, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexStagingBuffer, indexStagingBufferMemory);
    
    vkMapMemory(context->getDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, polygon.indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(context->getDevice(), indexStagingBufferMemory);
    
    createBuffer(context, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    
    copyBuffer(context, sync->getCommandPool(), indexStagingBuffer, indexBuffer, indexBufferSize);
    
    vkDestroyBuffer(context->getDevice(), indexStagingBuffer, nullptr);
    vkFreeMemory(context->getDevice(), indexStagingBufferMemory, nullptr);
    
    return true;
}

bool VulkanResources::createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    const uint32_t maxSets = 1024;
    
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    
    // Uniform buffers
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxSets;
    
    // Storage buffers (for keyframes)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = maxSets;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    
    if (vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
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
    if (vkAllocateDescriptorSets(context->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets!" << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        
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
        
        // Keyframe buffer binding
        VkDescriptorBufferInfo keyframeBufferInfo{};
        keyframeBufferInfo.buffer = keyframeBuffer;
        keyframeBufferInfo.offset = 0;
        keyframeBufferInfo.range = MAX_KEYFRAMES * KEYFRAME_SIZE;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &keyframeBufferInfo;
        
        vkUpdateDescriptorSets(context->getDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
    
    return true;
}

void VulkanResources::loadFunctions() {
    vkCreateBuffer = (PFN_vkCreateBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateBuffer");
    vkDestroyBuffer = (PFN_vkDestroyBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyBuffer");
    vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetBufferMemoryRequirements");
    vkBindBufferMemory = (PFN_vkBindBufferMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkBindBufferMemory");
    vkAllocateMemory = (PFN_vkAllocateMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateMemory");
    vkFreeMemory = (PFN_vkFreeMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkFreeMemory");
    vkMapMemory = (PFN_vkMapMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkMapMemory");
    vkUnmapMemory = (PFN_vkUnmapMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkUnmapMemory");
    vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateDescriptorPool");
    vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyDescriptorPool");
    vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateDescriptorSetLayout");
    vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyDescriptorSetLayout");
    vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateDescriptorSets");
    vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)context->vkGetDeviceProcAddr(context->getDevice(), "vkUpdateDescriptorSets");
    vkCreateImage = (PFN_vkCreateImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImage");
    vkDestroyImage = (PFN_vkDestroyImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyImage");
    vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetImageMemoryRequirements");
    vkBindImageMemory = (PFN_vkBindImageMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkBindImageMemory");
    vkCreateImageView = (PFN_vkCreateImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImageView");
    vkDestroyImageView = (PFN_vkDestroyImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyImageView");
    vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdCopyBuffer");
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
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    PFN_vkCreateImage vkCreateImage = (PFN_vkCreateImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImage");
    if (vkCreateImage(context->getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetImageMemoryRequirements");
    vkGetImageMemoryRequirements(context->getDevice(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties);

    PFN_vkAllocateMemory vkAllocateMemory = (PFN_vkAllocateMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateMemory");
    if (vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    PFN_vkBindImageMemory vkBindImageMemory = (PFN_vkBindImageMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkBindImageMemory");
    vkBindImageMemory(context->getDevice(), image, imageMemory, 0);
}

VkImageView VulkanResources::createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    PFN_vkCreateImageView vkCreateImageView = (PFN_vkCreateImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImageView");
    if (vkCreateImageView(context->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }

    return imageView;
}

void VulkanResources::createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    PFN_vkCreateBuffer vkCreateBuffer = (PFN_vkCreateBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateBuffer");
    if (vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetBufferMemoryRequirements");
    vkGetBufferMemoryRequirements(context->getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties);

    PFN_vkAllocateMemory vkAllocateMemory = (PFN_vkAllocateMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateMemory");
    if (vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    PFN_vkBindBufferMemory vkBindBufferMemory = (PFN_vkBindBufferMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkBindBufferMemory");
    vkBindBufferMemory(context->getDevice(), buffer, bufferMemory, 0);
}

void VulkanResources::copyBuffer(VulkanContext* context, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateCommandBuffers");
    vkAllocateCommandBuffers(context->getDevice(), &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkBeginCommandBuffer");
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdCopyBuffer");
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    PFN_vkEndCommandBuffer vkEndCommandBuffer = (PFN_vkEndCommandBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkEndCommandBuffer");
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    PFN_vkQueueSubmit vkQueueSubmit = (PFN_vkQueueSubmit)context->vkGetDeviceProcAddr(context->getDevice(), "vkQueueSubmit");
    vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    PFN_vkQueueWaitIdle vkQueueWaitIdle = (PFN_vkQueueWaitIdle)context->vkGetDeviceProcAddr(context->getDevice(), "vkQueueWaitIdle");
    vkQueueWaitIdle(context->getGraphicsQueue());
    
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)context->vkGetDeviceProcAddr(context->getDevice(), "vkFreeCommandBuffers");
    vkFreeCommandBuffers(context->getDevice(), commandPool, 1, &commandBuffer);
}

// Keyframe descriptor set methods
bool VulkanResources::createKeyframeDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    
    // Entity storage buffer (read-only)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 1;
    
    // Keyframe storage buffer (write-only)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    
    return vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &keyframeDescriptorPool) == VK_SUCCESS;
}

bool VulkanResources::createKeyframeDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    
    // Entity buffer binding (binding = 0, read-only)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Keyframe buffer binding (binding = 1, write-only)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();
    
    return this->vkCreateDescriptorSetLayout(context->getDevice(), &layoutInfo, nullptr, &keyframeDescriptorSetLayout) == VK_SUCCESS;
}

bool VulkanResources::createKeyframeDescriptorSets() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = keyframeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &keyframeDescriptorSetLayout;
    
    if (this->vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &keyframeDescriptorSet) != VK_SUCCESS) {
        return false;
    }
    
    // This will need to be updated with actual entity buffer from GPU entity manager
    // For now, just create the descriptor set - it will be updated later
    return true;
}

void VulkanResources::updateKeyframeDescriptorSet(VkBuffer entityBuffer) {
    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    
    // Entity buffer binding (binding = 0, read-only)
    VkDescriptorBufferInfo entityBufferInfo{};
    entityBufferInfo.buffer = entityBuffer;
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = VK_WHOLE_SIZE;
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = keyframeDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &entityBufferInfo;
    
    // Keyframe buffer binding (binding = 1, write-only)
    VkDescriptorBufferInfo keyframeBufferInfo{};
    keyframeBufferInfo.buffer = keyframeBuffer;
    keyframeBufferInfo.offset = 0;
    keyframeBufferInfo.range = MAX_KEYFRAMES * KEYFRAME_SIZE;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = keyframeDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &keyframeBufferInfo;
    
    this->vkUpdateDescriptorSets(context->getDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}