#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "../core/vulkan_raii.h"
#include "resource_handle.h"

class VulkanContext;
class BufferFactory;

// Consolidated graphics pipeline resource management
// Merged from GraphicsResourceFacade for simplified architecture
class GraphicsResourceManager {
public:
    GraphicsResourceManager();
    ~GraphicsResourceManager();
    
    bool initialize(const VulkanContext& context, BufferFactory* bufferFactory);
    void cleanup();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // High-level resource operations (consolidated from facade)
    bool createAllGraphicsResources();
    bool recreateGraphicsResources();
    
    // Individual resource creation
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
    VkDescriptorPool getDescriptorPool() const { return graphicsDescriptorPool.get(); }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return graphicsDescriptorSets; }
    
    // Resource state queries (from facade)
    bool areResourcesCreated() const;
    bool areDescriptorsCreated() const;
    bool needsRecreation() const { return resourcesNeedRecreation; }
    
    // Memory optimization (from facade)
    bool optimizeGraphicsMemoryUsage();
    VkDeviceSize getGraphicsMemoryFootprint() const;
    
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
    
    // State tracking (from facade)
    bool resourcesNeedRecreation = false;
    
    // Internal helpers
    void markForRecreation() { resourcesNeedRecreation = true; }
    void clearRecreationFlag() { resourcesNeedRecreation = false; }
};