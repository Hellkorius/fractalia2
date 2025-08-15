#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_constants.h"
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
    

    
    // Get maximum instances that can fit in buffer
    uint32_t getMaxInstances() const { return MAX_INSTANCES; }


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
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
    void* instanceBufferMapped = nullptr;
    
    
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    

    VulkanFunctionLoader* loader = nullptr;
};