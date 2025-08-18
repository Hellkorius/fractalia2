#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "../core/vulkan_raii.h"
#include "resource_handle.h"

class VulkanContext;
class BufferFactory;

// Graphics pipeline-specific resource management
class GraphicsResourceManager {
public:
    GraphicsResourceManager();
    ~GraphicsResourceManager();
    
    bool initialize(const VulkanContext& context, BufferFactory* bufferFactory);
    void cleanup();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Graphics resource creation
    bool createUniformBuffers();
    bool createTriangleBuffers();
    bool createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);
    
    // Descriptor set updates
    bool updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer);
    bool updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer);
    bool updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer);
    
    // Recreation for swapchain rebuild
    bool recreateGraphicsDescriptors();
    
    // Getters for graphics resources
    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }
    VkBuffer getVertexBuffer() const { return vertexBufferHandle.buffer.get(); }
    VkBuffer getIndexBuffer() const { return indexBufferHandle.buffer.get(); }
    uint32_t getIndexCount() const { return indexCount; }
    VkDescriptorPool getGraphicsDescriptorPool() const { return graphicsDescriptorPool.get(); }
    const std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() const { return graphicsDescriptorSets; }
    
    // Cleanup before context destruction
    void cleanupBeforeContextDestruction();

private:
    const VulkanContext* context = nullptr;
    BufferFactory* bufferFactory = nullptr;
    
    // Graphics pipeline resources
    std::vector<ResourceHandle> uniformBufferHandles;
    std::vector<VkBuffer> uniformBuffers;  // For compatibility
    std::vector<void*> uniformBuffersMapped;
    
    ResourceHandle vertexBufferHandle;
    ResourceHandle indexBufferHandle;
    uint32_t indexCount = 0;
    
    vulkan_raii::DescriptorPool graphicsDescriptorPool;
    std::vector<VkDescriptorSet> graphicsDescriptorSets;
    
    // Track descriptor layout for recreation
    VkDescriptorSetLayout cachedDescriptorLayout = VK_NULL_HANDLE;
};