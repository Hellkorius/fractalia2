#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include "resource_handle.h"
#include "command_executor.h"
#include "resource_context_bridge.h"

class VulkanContext;
class QueueManager;
class ResourceFactory;
class TransferManager;
class MemoryAllocator;
class DescriptorPoolManager;
class GraphicsResourceManager;
class BufferManager;
class StagingBufferPool;

// Lightweight coordination only - delegates to specialized managers
class ResourceCoordinator {
public:
    ResourceCoordinator();
    ~ResourceCoordinator();
    
    bool initialize(const VulkanContext& context, QueueManager* queueManager);
    void cleanup();
    void cleanupBeforeContextDestruction();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Resource creation (delegates to ResourceFactory)
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
    
    // Transfer operations (delegates to TransferManager)
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, 
                           VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, 
                                                     VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Manager access for advanced operations
    MemoryAllocator* getMemoryAllocator() const;
    ResourceFactory* getResourceFactory() const;
    TransferManager* getTransferManager() const;
    DescriptorPoolManager* getDescriptorPoolManager() const;
    GraphicsResourceManager* getGraphicsManager() const;
    BufferManager* getBufferManager() const;
    CommandExecutor* getCommandExecutor() { return &executor; }
    const CommandExecutor* getCommandExecutor() const { return &executor; }
    
    // Legacy compatibility - staging buffer direct access
    StagingBufferPool& getStagingBuffer();
    const StagingBufferPool& getStagingBuffer() const;
    
    // Graphics resource convenience methods
    const std::vector<VkBuffer>& getUniformBuffers() const;
    const std::vector<void*>& getUniformBuffersMapped() const;
    
    // Statistics and monitoring (delegates to MemoryAllocator)
    bool isUnderMemoryPressure() const;
    bool attemptMemoryRecovery();
    
    // Simplified memory statistics interface
    VkDeviceSize getTotalAllocatedMemory() const;
    VkDeviceSize getAvailableMemory() const;
    uint32_t getAllocationCount() const;
    
    // Performance optimization
    bool optimizeResources();

private:
    const VulkanContext* context = nullptr;
    CommandExecutor executor;
    
    // Bridge for breaking circular dependencies
    std::unique_ptr<ResourceContextBridge> contextBridge;
    
    // Focused managers
    std::unique_ptr<MemoryAllocator> memoryAllocator;
    std::unique_ptr<ResourceFactory> resourceFactory;
    std::unique_ptr<TransferManager> transferManager;
    std::unique_ptr<DescriptorPoolManager> descriptorPoolManager;
    std::unique_ptr<GraphicsResourceManager> graphicsResourceManager;
    std::unique_ptr<BufferManager> bufferManager;
    
    // Initialization helpers
    bool initializeManagers(QueueManager* queueManager);
    void setupManagerDependencies();
    void cleanupManagers();
};