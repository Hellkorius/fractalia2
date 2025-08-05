#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"

class VulkanResources {
public:
    VulkanResources();
    ~VulkanResources();

    bool initialize(VulkanContext* context);
    void cleanup();
    
    bool createUniformBuffers();
    bool createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);

    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }
    
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }

    static uint32_t findMemoryType(VulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static void createImage(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits numSamples,
                           VkImage& image, VkDeviceMemory& imageMemory);
    static VkImageView createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    static void createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    static const int MAX_FRAMES_IN_FLIGHT = 2;

private:
    VulkanContext* context = nullptr;
    
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    // Function pointers for resource operations
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    
    void loadFunctions();
};