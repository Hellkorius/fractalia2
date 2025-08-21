#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class ResourceContext;
class GraphicsResourceManager;

// High-level facade for graphics pipeline resources
// Hides the complexity of graphics-specific operations from ResourceContext
class GraphicsResourceFacade {
public:
    GraphicsResourceFacade();
    ~GraphicsResourceFacade();
    
    bool initialize(ResourceContext* resourceContext, GraphicsResourceManager* graphicsManager);
    void cleanup();
    
    // Context access
    ResourceContext* getResourceContext() const { return resourceContext; }
    GraphicsResourceManager* getGraphicsManager() const { return graphicsManager; }
    
    // High-level graphics resource operations
    bool createAllGraphicsResources();
    bool recreateGraphicsResources(); // For swapchain rebuild
    void cleanupGraphicsResources();
    
    // Uniform buffer management
    bool createUniformBuffers();
    const std::vector<VkBuffer>& getUniformBuffers() const;
    const std::vector<void*>& getUniformBuffersMapped() const;
    
    // Geometry buffers
    bool createGeometryBuffers();
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    uint32_t getIndexCount() const;
    
    // Descriptor management
    bool createDescriptorResources(VkDescriptorSetLayout descriptorSetLayout);
    bool updateDescriptorSetsForEntityRendering(VkBuffer entityBuffer, VkBuffer positionBuffer);
    bool updateDescriptorSetsForPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer);
    VkDescriptorPool getDescriptorPool() const;
    const std::vector<VkDescriptorSet>& getDescriptorSets() const;
    
    // Resource state queries
    bool areResourcesCreated() const;
    bool areDescriptorsCreated() const;
    bool needsRecreation() const;
    bool isInitialized() const { return initialized; }
    
    // Advanced operations
    struct GraphicsResourceInfo {
        bool uniformBuffersCreated = false;
        bool geometryBuffersCreated = false;
        bool descriptorPoolCreated = false;
        bool descriptorSetsCreated = false;
        uint32_t uniformBufferCount = 0;
        uint32_t descriptorSetCount = 0;
        VkDeviceSize totalUniformBufferSize = 0;
        VkDeviceSize totalGeometryBufferSize = 0;
    };
    
    GraphicsResourceInfo getResourceInfo() const;
    
    // Batch operations for efficiency
    struct ResourceCreationBatch {
        bool createUniformBuffers = true;
        bool createGeometryBuffers = true;
        bool createDescriptorPool = true;
        bool createDescriptorSets = true;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    };
    
    bool createResourcesBatch(const ResourceCreationBatch& batch);
    
    // Memory optimization
    bool optimizeGraphicsMemoryUsage();
    VkDeviceSize getGraphicsMemoryFootprint() const;
    
private:
    ResourceContext* resourceContext = nullptr;
    GraphicsResourceManager* graphicsManager = nullptr;
    
    // State tracking
    bool initialized = false;
    bool resourcesNeedRecreation = false;
    
    // Internal helpers
    bool validateDependencies() const;
    void markForRecreation() { resourcesNeedRecreation = true; }
    void clearRecreationFlag() { resourcesNeedRecreation = false; }
};