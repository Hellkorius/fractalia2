#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "../PolygonFactory.h"

class VulkanSync;

class VulkanResources {
public:
    VulkanResources();
    ~VulkanResources();

    bool initialize(VulkanContext* context, VulkanSync* sync);
    void cleanup();
    
    bool createUniformBuffers();
    bool createVertexBuffer();
    bool createIndexBuffer();
    bool createInstanceBuffers();
    bool createMultiShapeBuffers();
    bool createPolygonBuffers(const PolygonMesh& polygon);
    bool createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);

    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }
    
    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer() const { return indexBuffer; }
    VkBuffer getTriangleVertexBuffer() const { return triangleVertexBuffer; }
    VkBuffer getTriangleIndexBuffer() const { return triangleIndexBuffer; }
    VkBuffer getSquareVertexBuffer() const { return squareVertexBuffer; }
    VkBuffer getSquareIndexBuffer() const { return squareIndexBuffer; }
    uint32_t getTriangleIndexCount() const { return triangleIndexCount; }
    uint32_t getSquareIndexCount() const { return squareIndexCount; }
    const std::vector<VkBuffer>& getInstanceBuffers() const { return instanceBuffers; }
    const std::vector<void*>& getInstanceBuffersMapped() const { return instanceBuffersMapped; }
    uint32_t getIndexCount() const { return indexCount; }
    
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }

    static uint32_t findMemoryType(VulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static void createImage(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits numSamples,
                           VkImage& image, VkDeviceMemory& imageMemory);
    static VkImageView createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    static void createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    static void copyBuffer(VulkanContext* context, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    static const int MAX_FRAMES_IN_FLIGHT = 2;

private:
    VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
    
    // Separate buffers for different shapes
    VkBuffer triangleVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory triangleVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer triangleIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory triangleIndexBufferMemory = VK_NULL_HANDLE;
    uint32_t triangleIndexCount = 0;
    
    VkBuffer squareVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory squareVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer squareIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory squareIndexBufferMemory = VK_NULL_HANDLE;
    uint32_t squareIndexCount = 0;
    
    static const int STAGING_BUFFER_COUNT = 4;
    static const VkDeviceSize STAGING_BUFFER_SIZE = 1024 * 1024;
    std::vector<VkBuffer> instanceBuffers;
    std::vector<VkDeviceMemory> instanceBuffersMemory;
    std::vector<void*> instanceBuffersMapped;
    int currentStagingBuffer = 0;
    
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
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
    
    void loadFunctions();
};