#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "../PolygonFactory.h"

class VulkanSync;
class VulkanFunctionLoader;

class VulkanResources {
public:
    VulkanResources();
    ~VulkanResources();

    bool initialize(VulkanContext* context, VulkanSync* sync, VulkanFunctionLoader* loader);
    void cleanup();
    
    bool createUniformBuffers();
    bool createVertexBuffer();
    bool createIndexBuffer();
    bool createInstanceBuffer();
    bool createTriangleBuffers();
    bool createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);

    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }
    
    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer() const { return indexBuffer; }
    uint32_t getIndexCount() const { return indexCount; }
    VkBuffer getInstanceBuffer() const { return instanceBuffer; }
    void* getInstanceBufferMapped() const { return instanceBufferMapped; }
    
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }
    

    static uint32_t findMemoryType(VulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static void createImage(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits numSamples,
                           VkImage& image, VkDeviceMemory& imageMemory);
    static VkImageView createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    static void createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    
    // Get maximum instances that can fit in buffer
    uint32_t getMaxInstances() const { return MAX_INSTANCES; }
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
    
    // Single instance buffer for simplified management
    static const VkDeviceSize INSTANCE_BUFFER_SIZE = 32 * 1024 * 1024; // 32MB for GPU entities
    static const uint32_t MAX_INSTANCES = INSTANCE_BUFFER_SIZE / 128; // 128 bytes per GPUEntity
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
    void* instanceBufferMapped = nullptr;
    
    
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    

    VulkanFunctionLoader* loader = nullptr;
};