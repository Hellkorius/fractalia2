#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class VulkanContext;
class VulkanFunctionLoader;

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
    
private:
    const VulkanContext* context = nullptr;
    ResourceHandle ringBuffer;
    VkDeviceSize currentOffset = 0;
    VkDeviceSize totalSize = 0;
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
};