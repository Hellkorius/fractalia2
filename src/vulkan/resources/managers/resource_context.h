#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>
#include "../core/resource_handle.h"
#include "../core/command_executor.h"
#include "../core/resource_context_interface.h"
#include "../../core/vulkan_raii.h"

class VulkanContext;
class QueueManager;
class ResourceCoordinator;
class DescriptorPoolManager;
class GraphicsResourceManager;

// New lightweight ResourceContext that delegates to ResourceCoordinator
// Maintains backward compatibility while eliminating God Object pattern
class ResourceContext : public IResourceContext {
public:
    ResourceContext();
    ~ResourceContext();
    
    bool initialize(const VulkanContext& context, QueueManager* queueManager);
    void cleanup();
    void cleanupBeforeContextDestruction();
    
    // Context access
    const VulkanContext* getContext() const override;
    
    // Core resource creation (delegates to ResourceCoordinator)
    ResourceHandle createBuffer(VkDeviceSize size, 
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties) override;
    
    ResourceHandle createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) override;
    
    ResourceHandle createImage(uint32_t width, uint32_t height,
                              VkFormat format,
                              VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    
    ResourceHandle createImageView(const ResourceHandle& imageHandle,
                                  VkFormat format,
                                  VkImageAspectFlags aspectFlags);
    
    void destroyResource(ResourceHandle& handle) override;
    
    // Transfer operations (delegates to ResourceCoordinator)
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) override;
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Descriptor management (delegates to DescriptorPoolManager)
    vulkan_raii::DescriptorPool createDescriptorPool();
    vulkan_raii::DescriptorPool createDescriptorPoolWithConfig(uint32_t maxSets, 
                                                               uint32_t uniformBufferCount = 1024,
                                                               uint32_t storageBufferCount = 1024);
    void destroyDescriptorPool(VkDescriptorPool pool);
    
    // Graphics resources (delegates to GraphicsResourceManager)
    bool createGraphicsResources();
    bool recreateGraphicsResources();
    bool updateGraphicsDescriptors(VkBuffer entityBuffer, VkBuffer positionBuffer);
    
    // Graphics resource individual creation (for legacy compatibility)
    bool createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);
    
    // Getters - delegates to appropriate managers
    const std::vector<VkBuffer>& getUniformBuffers() const;
    const std::vector<void*>& getUniformBuffersMapped() const;
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    uint32_t getIndexCount() const;
    VkDescriptorPool getGraphicsDescriptorPool() const;
    const std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() const;
    
    // Manager access for advanced operations
    class MemoryAllocator* getMemoryAllocator() const override;
    class BufferFactory* getBufferFactory() const override;
    CommandExecutor* getCommandExecutor() const override;
    class BufferManager* getBufferManager() const;
    class GraphicsResourceManager* getGraphicsManager() const;
    
    // Legacy compatibility - staging buffer direct access
    class StagingBufferPool& getStagingBuffer();
    const class StagingBufferPool& getStagingBuffer() const;
    
    // Statistics and monitoring (delegates to ResourceCoordinator)
    bool isUnderMemoryPressure() const;
    bool attemptMemoryRecovery();
    
    // Memory statistics - simplified interface
    VkDeviceSize getTotalAllocatedMemory() const;
    VkDeviceSize getAvailableMemory() const;
    uint32_t getAllocationCount() const;
    
    // Legacy memory stats structure for backward compatibility
    struct SimpleMemoryStats {
        VkDeviceSize totalAllocated = 0;
        VkDeviceSize totalFreed = 0;
        uint32_t activeAllocations = 0;
        VkDeviceSize peakUsage = 0;
        uint32_t failedAllocations = 0;
        bool memoryPressure = false;
        float fragmentationRatio = 0.0f;
    };
    SimpleMemoryStats getMemoryStats() const;
    
    // Performance optimization
    bool optimizeResources();

private:
    // Focused coordination through ResourceCoordinator
    std::unique_ptr<ResourceCoordinator> coordinator;
    
    // Specialized managers for operations not handled by ResourceCoordinator
    std::unique_ptr<DescriptorPoolManager> descriptorPoolManager;
    std::unique_ptr<GraphicsResourceManager> graphicsResourceManager;
    
    bool initialized = false;
};