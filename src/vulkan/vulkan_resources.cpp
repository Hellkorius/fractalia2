#include "vulkan_resources.h"
#include <iostream>
#include <stdexcept>

VulkanResources::VulkanResources() {
}

VulkanResources::~VulkanResources() {
    cleanup();
}

bool VulkanResources::initialize(VulkanContext* context) {
    this->context = context;
    
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
    
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context->getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

bool VulkanResources::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(float) * 16;
    
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

bool VulkanResources::createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    
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
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = 64;
        
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(context->getDevice(), 1, &descriptorWrite, 0, nullptr);
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
    vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateDescriptorSets");
    vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)context->vkGetDeviceProcAddr(context->getDevice(), "vkUpdateDescriptorSets");
    vkCreateImage = (PFN_vkCreateImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImage");
    vkDestroyImage = (PFN_vkDestroyImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyImage");
    vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetImageMemoryRequirements");
    vkBindImageMemory = (PFN_vkBindImageMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkBindImageMemory");
    vkCreateImageView = (PFN_vkCreateImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImageView");
    vkDestroyImageView = (PFN_vkDestroyImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyImageView");
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