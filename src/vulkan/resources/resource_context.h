#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "../core/vulkan_constants.h"
#include "../core/vulkan_raii.h"

class VulkanContext;
class VulkanFunctionLoader;
class ResourceContext;
class QueueManager;

#include "command_executor.h"
#include "memory_allocator.h"
#include "buffer_factory.h"
#include "descriptor_pool_manager.h"
#include "graphics_resource_manager.h"
#include "resource_handle.h"

// Staging ring buffer for efficient CPU->GPU transfers
class StagingRingBuffer {
public:
    bool initialize(const VulkanContext& context, VkDeviceSize size);
    void cleanup();
    
    struct StagingRegion {
        void* mappedData;
        VkBuffer buffer;  // Keep raw handle for compatibility
        VkDeviceSize offset;
        VkDeviceSize size;
        
        // Validity check
        bool isValid() const { return mappedData != nullptr && buffer != VK_NULL_HANDLE; }
    };
    
    // RAII wrapper for staging region allocation
    class StagingRegionGuard {
    public:
        StagingRegionGuard(StagingRingBuffer* buffer, VkDeviceSize size, VkDeviceSize alignment = 1)
            : stagingBuffer(buffer), region(buffer ? buffer->allocate(size, alignment) : StagingRegion{}) {}
        
        ~StagingRegionGuard() = default; // Ring buffer manages its own memory
        
        StagingRegionGuard(const StagingRegionGuard&) = delete;
        StagingRegionGuard& operator=(const StagingRegionGuard&) = delete;
        
        StagingRegionGuard(StagingRegionGuard&& other) noexcept 
            : stagingBuffer(other.stagingBuffer), region(other.region) {
            other.stagingBuffer = nullptr;
            other.region = {};
        }
        
        StagingRegionGuard& operator=(StagingRegionGuard&& other) noexcept {
            if (this != &other) {
                stagingBuffer = other.stagingBuffer;
                region = other.region;
                other.stagingBuffer = nullptr;
                other.region = {};
            }
            return *this;
        }
        
        const StagingRegion& get() const { return region; }
        bool isValid() const { return region.isValid(); }
        
        // Access operators
        const StagingRegion* operator->() const { return &region; }
        const StagingRegion& operator*() const { return region; }
        
    private:
        StagingRingBuffer* stagingBuffer;
        StagingRegion region;
    };
    
    StagingRegion allocate(VkDeviceSize size, VkDeviceSize alignment = 1);
    StagingRegionGuard allocateGuarded(VkDeviceSize size, VkDeviceSize alignment = 1);
    void reset(); // Reset to beginning of ring buffer
    
    // Advanced memory management
    bool tryDefragment(); // Attempt to reduce fragmentation
    VkDeviceSize getFragmentedBytes() const; // Track wasted space
    bool isFragmentationCritical() const; // >50% fragmented
    
    // Getter for buffer handle
    VkBuffer getBuffer() const { return ringBuffer.buffer.get(); }
    
private:
    const VulkanContext* context = nullptr;
    ResourceHandle ringBuffer;
    VkDeviceSize currentOffset = 0;
    VkDeviceSize totalSize = 0;
    
    // Fragmentation tracking
    VkDeviceSize totalWastedBytes = 0;
    uint32_t wrapAroundCount = 0;
    VkDeviceSize largestFreeBlock = 0;
};

// GPU buffer with integrated staging support for compute operations
class GPUBufferRing {
public:
    GPUBufferRing() = default;
    ~GPUBufferRing();
    
    bool initialize(ResourceContext* resourceContext, VkDeviceSize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void cleanup();
    
    // Buffer access
    VkBuffer getBuffer() const { return storageHandle ? storageHandle->buffer.get() : VK_NULL_HANDLE; }
    void* getMappedData() const { return storageHandle ? storageHandle->mappedData : nullptr; }
    VkDeviceSize getSize() const { return bufferSize; }
    bool isValid() const { return storageHandle && storageHandle->isValid(); }
    
    // Staging operations for device-local buffers
    bool addData(const void* data, VkDeviceSize size, VkDeviceSize alignment = alignof(std::max_align_t));
    void flushToGPU(VkDeviceSize dstOffset = 0); // dstOffset for where to write in destination
    void resetStaging();
    bool hasPendingData() const { return needsUpload; }
    
    // Direct access to handle for special operations
    ResourceHandle* getHandle() { return storageHandle.get(); }
    const ResourceHandle* getHandle() const { return storageHandle.get(); }
    
private:
    std::unique_ptr<ResourceHandle> storageHandle;
    ResourceContext* resourceContext = nullptr;
    VkDeviceSize bufferSize = 0;
    
    // Staging state for device-local buffers
    VkDeviceSize stagingBytesWritten = 0;
    VkDeviceSize stagingStartOffset = 0;
    bool needsUpload = false;
    bool isDeviceLocal = false;
};

// Lightweight centralized resource allocation manager
class ResourceContext {
public:
    ResourceContext();
    ~ResourceContext();
    
    bool initialize(const VulkanContext& context, VkCommandPool commandPool = VK_NULL_HANDLE);
    bool initialize(const VulkanContext& context, QueueManager* queueManager);
    void cleanup();
    
    // Update command pool without reinitializing everything
    bool updateCommandPool(VkCommandPool newCommandPool);
    
    // Cleanup method for proper destruction order
    void cleanupBeforeContextDestruction();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Buffer creation helpers
    ResourceHandle createBuffer(VkDeviceSize size, 
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties);
    
    ResourceHandle createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    // Image creation helpers  
    ResourceHandle createImage(uint32_t width, uint32_t height,
                              VkFormat format,
                              VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    
    ResourceHandle createImageView(const ResourceHandle& imageHandle,
                                  VkFormat format,
                                  VkImageAspectFlags aspectFlags);
    
    // Resource destruction
    void destroyResource(ResourceHandle& handle);
    
    // Staging operations
    StagingRingBuffer& getStagingBuffer() { return stagingBuffer; }
    void copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Async staging operations using transfer queue
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Descriptor management (delegated to DescriptorPoolManager)
    using DescriptorPoolConfig = DescriptorPoolManager::DescriptorPoolConfig;
    vulkan_raii::DescriptorPool createDescriptorPool(const DescriptorPoolConfig& config);
    vulkan_raii::DescriptorPool createDescriptorPool(); // Overload with default config
    void destroyDescriptorPool(VkDescriptorPool pool);
    
    // Graphics pipeline resources (moved from VulkanResources)
    bool createUniformBuffers();
    bool createTriangleBuffers();
    bool createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);
    bool updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer);
    bool updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer);
    bool updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer);
    
    // Recreation for swapchain rebuild
    bool recreateGraphicsDescriptors();
    
    // Getters for graphics resources (delegated to GraphicsResourceManager)
    const std::vector<VkBuffer>& getUniformBuffers() const;
    const std::vector<void*>& getUniformBuffersMapped() const;
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    uint32_t getIndexCount() const;
    VkDescriptorPool getGraphicsDescriptorPool() const;
    const std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() const;
    
    // Statistics and debugging (delegated to MemoryAllocator)
    using MemoryStats = MemoryAllocator::MemoryStats;
    MemoryStats getMemoryStats() const;
    
    // Memory pressure management (delegated to MemoryAllocator)
    bool isUnderMemoryPressure() const;
    bool attemptMemoryRecovery();
    
private:
    const VulkanContext* context = nullptr;
    StagingRingBuffer stagingBuffer;
    CommandExecutor executor;
    
    // Specialized managers
    std::unique_ptr<MemoryAllocator> memoryAllocator;
    std::unique_ptr<BufferFactory> bufferFactory;
    std::unique_ptr<DescriptorPoolManager> descriptorPoolManager;
    std::unique_ptr<GraphicsResourceManager> graphicsResourceManager;
    
    // Resource tracking
    std::vector<std::function<void()>> cleanupCallbacks;
};