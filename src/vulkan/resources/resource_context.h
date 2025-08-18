#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "../core/vulkan_constants.h"

class VulkanContext;
class VulkanFunctionLoader;
class ResourceContext;

#include "command_executor.h"

// VMA allocator wrapper - simplified interface without external dependency
class VmaAllocator_Impl;
using VmaAllocator = VmaAllocator_Impl*;
using VmaAllocation = void*;

// Resource handle combining buffer/image with allocation
struct ResourceHandle {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    void* mappedData = nullptr;
    VkDeviceSize size = 0;
    
    bool isValid() const { return buffer != VK_NULL_HANDLE || image != VK_NULL_HANDLE; }
};

// Staging ring buffer for efficient CPU->GPU transfers
class StagingRingBuffer {
public:
    bool initialize(const VulkanContext& context, VkDeviceSize size);
    void cleanup();
    
    struct StagingRegion {
        void* mappedData;
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
    
    StagingRegion allocate(VkDeviceSize size, VkDeviceSize alignment = 1);
    void reset(); // Reset to beginning of ring buffer
    
    // Getter for buffer handle
    VkBuffer getBuffer() const { return ringBuffer.buffer; }
    
private:
    const VulkanContext* context = nullptr;
    ResourceHandle ringBuffer;
    VkDeviceSize currentOffset = 0;
    VkDeviceSize totalSize = 0;
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
    VkBuffer getBuffer() const { return storageHandle ? storageHandle->buffer : VK_NULL_HANDLE; }
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
    void cleanup();
    
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
    
    // Descriptor management (bindless-ready)
    struct DescriptorPoolConfig {
        uint32_t maxSets = 1024;
        uint32_t uniformBuffers = 1024;
        uint32_t storageBuffers = 1024;
        uint32_t sampledImages = 1024;
        uint32_t storageImages = 512;
        uint32_t samplers = 512;
        bool allowFreeDescriptorSets = true;
        bool bindlessReady = false; // Future-proof for bindless
    };
    
    VkDescriptorPool createDescriptorPool(const DescriptorPoolConfig& config);
    VkDescriptorPool createDescriptorPool(); // Overload with default config
    void destroyDescriptorPool(VkDescriptorPool pool);
    
    // Graphics pipeline resources (moved from VulkanResources)
    bool createUniformBuffers();
    bool createTriangleBuffers();
    bool createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout);
    bool updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer);
    bool updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer);
    bool updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer);
    
    // Getters for graphics resources
    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }
    VkBuffer getVertexBuffer() const { return vertexBufferHandle.buffer; }
    VkBuffer getIndexBuffer() const { return indexBufferHandle.buffer; }
    uint32_t getIndexCount() const { return indexCount; }
    VkDescriptorPool getGraphicsDescriptorPool() const { return graphicsDescriptorPool; }
    const std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() const { return graphicsDescriptorSets; }
    
    // Statistics and debugging
    struct MemoryStats {
        VkDeviceSize totalAllocated = 0;
        VkDeviceSize totalFreed = 0;
        uint32_t activeAllocations = 0;
    };
    
    MemoryStats getMemoryStats() const { return memoryStats; }
    
private:
    const VulkanContext* context = nullptr;
    VmaAllocator allocator = nullptr;
    StagingRingBuffer stagingBuffer;
    MemoryStats memoryStats;
    CommandExecutor executor;
    
    // Internal VMA wrapper functions
    bool initializeVMA();
    void cleanupVMA();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Resource tracking
    std::vector<std::function<void()>> cleanupCallbacks;
    
    // Graphics pipeline resources (moved from VulkanResources)
    std::vector<ResourceHandle> uniformBufferHandles;
    std::vector<VkBuffer> uniformBuffers;  // For compatibility
    std::vector<void*> uniformBuffersMapped;
    
    ResourceHandle vertexBufferHandle;
    ResourceHandle indexBufferHandle;
    uint32_t indexCount = 0;
    
    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> graphicsDescriptorSets;
};