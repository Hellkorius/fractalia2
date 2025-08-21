#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>
#include "../core/vulkan_constants.h"
#include "../core/vulkan_raii.h"

class VulkanContext;
class QueueManager;

// Forward declarations for new modular managers
class MemoryAllocator;
class BufferFactory;
class DescriptorPoolManager;
class GraphicsResourceManager;
class StagingBufferManager;
class StagingRingBuffer;
class GPUBufferManager;
class TransferManager;

#include "resource_handle.h"
#include "command_executor.h"

// Lightweight centralized resource coordination manager
// Delegates to specialized managers for actual implementation
class ResourceContext {
public:
    ResourceContext();
    ~ResourceContext();
    
    bool initialize(const VulkanContext& context, QueueManager* queueManager);
    void cleanup();
    
    // Cleanup method for proper destruction order
    void cleanupBeforeContextDestruction();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Core resource creation (delegated to BufferFactory)
    ResourceHandle createBuffer(VkDeviceSize size, 
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties);
    
    ResourceHandle createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    ResourceHandle createImage(uint32_t width, uint32_t height,
                              VkFormat format,
                              VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    
    ResourceHandle createImageView(const ResourceHandle& imageHandle,
                                  VkFormat format,
                                  VkImageAspectFlags aspectFlags);
    
    void destroyResource(ResourceHandle& handle);
    
    // Transfer operations (delegated to TransferManager)
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Descriptor management (delegated to DescriptorPoolManager)
    vulkan_raii::DescriptorPool createDescriptorPool(); // Uses default config
    void destroyDescriptorPool(VkDescriptorPool pool);
    
    // Advanced descriptor pool creation with custom config - implemented in .cpp
    vulkan_raii::DescriptorPool createDescriptorPoolWithConfig(uint32_t maxSets, 
                                                               uint32_t uniformBufferCount = 1024,
                                                               uint32_t storageBufferCount = 1024);
    
    // Graphics resources (delegated to GraphicsResourceManager)
    bool createGraphicsResources();
    bool recreateGraphicsResources();
    bool updateGraphicsDescriptors(VkBuffer entityBuffer, VkBuffer positionBuffer);
    
    // Individual graphics resource creation (for legacy compatibility)
    bool createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);
    
    // Getters
    const std::vector<VkBuffer>& getUniformBuffers() const;
    const std::vector<void*>& getUniformBuffersMapped() const;
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    uint32_t getIndexCount() const;
    VkDescriptorPool getGraphicsDescriptorPool() const;
    const std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() const;
    
    // Manager access for advanced operations
    MemoryAllocator* getMemoryAllocator() const { return memoryAllocator.get(); }
    BufferFactory* getBufferFactory() const { return bufferFactory.get(); }
    StagingBufferManager* getStagingManager() const { return stagingManager.get(); }
    GPUBufferManager* getGPUBufferManager() const { return gpuBufferManager.get(); }
    TransferManager* getTransferManager() const { return transferManager.get(); }
    GraphicsResourceManager* getGraphicsManager() const { return graphicsResourceManager.get(); }
    
    // Legacy compatibility - staging buffer direct access
    StagingRingBuffer& getStagingBuffer();
    const StagingRingBuffer& getStagingBuffer() const;
    
    // Statistics and monitoring (delegated to MemoryAllocator)
    bool isUnderMemoryPressure() const;
    bool attemptMemoryRecovery();
    
    // Memory statistics - simplified interface to avoid incomplete types
    VkDeviceSize getTotalAllocatedMemory() const;
    VkDeviceSize getAvailableMemory() const;
    uint32_t getAllocationCount() const;
    
    // Legacy memory stats access (for backward compatibility)
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
    bool optimizeResources(); // Optimize all managed resources
    
private:
    const VulkanContext* context = nullptr;
    CommandExecutor executor;
    
    // Specialized managers - order matters for initialization/cleanup
    std::unique_ptr<MemoryAllocator> memoryAllocator;
    std::unique_ptr<BufferFactory> bufferFactory;
    std::unique_ptr<DescriptorPoolManager> descriptorPoolManager;
    std::unique_ptr<GraphicsResourceManager> graphicsResourceManager;
    std::unique_ptr<StagingBufferManager> stagingManager;
    std::unique_ptr<GPUBufferManager> gpuBufferManager;
    std::unique_ptr<TransferManager> transferManager;
    
    // Resource tracking
    std::vector<std::function<void()>> cleanupCallbacks;
    
    // Initialization helpers
    bool initializeManagers(QueueManager* queueManager);
    void setupManagerDependencies();
    void cleanupManagers();
};