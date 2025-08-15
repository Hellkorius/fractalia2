#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_constants.h"
#include "resource_context.h"
#include "../PolygonFactory.h"

class VulkanSync;

class VulkanResources {
public:
    VulkanResources();
    ~VulkanResources();

    bool initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext);
    void cleanup();
    
    bool createUniformBuffers();
    bool createTriangleBuffers();
    bool createDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);

    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }
    
    VkBuffer getVertexBuffer() const { return vertexBufferHandle.buffer; }
    VkBuffer getIndexBuffer() const { return indexBufferHandle.buffer; }
    uint32_t getIndexCount() const { return indexCount; }
    
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }
    



private:
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    std::vector<ResourceHandle> uniformBufferHandles;
    std::vector<VkBuffer> uniformBuffers;  // For compatibility
    std::vector<void*> uniformBuffersMapped;
    
    ResourceHandle vertexBufferHandle;
    ResourceHandle indexBufferHandle;
    uint32_t indexCount = 0;
    
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    

};