#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>

class VulkanContext;

// Need complete ResourceHandle definition for member variables
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

// Staging buffer management with multiple ring buffers and automatic sizing
class StagingBufferManager {
public:
    StagingBufferManager();
    ~StagingBufferManager();
    
    bool initialize(const VulkanContext& context, VkDeviceSize initialSize = 16 * 1024 * 1024); // 16MB default
    void cleanup();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Primary staging buffer access
    StagingRingBuffer& getPrimaryBuffer() { return primaryBuffer; }
    const StagingRingBuffer& getPrimaryBuffer() const { return primaryBuffer; }
    
    // High-level allocation interface
    StagingRingBuffer::StagingRegion allocate(VkDeviceSize size, VkDeviceSize alignment = 1);
    StagingRingBuffer::StagingRegionGuard allocateGuarded(VkDeviceSize size, VkDeviceSize alignment = 1);
    
    // Memory management
    void reset(); // Reset all buffers
    bool tryDefragment(); // Defragment all buffers
    
    // Statistics and monitoring
    struct StagingStats {
        VkDeviceSize totalSize = 0;
        VkDeviceSize fragmentedBytes = 0;
        uint32_t allocationCount = 0;
        float fragmentationRatio = 0.0f;
        bool isFragmentationCritical = false;
    };
    
    StagingStats getStats() const;
    bool isUnderMemoryPressure() const;
    
private:
    const VulkanContext* context = nullptr;
    StagingRingBuffer primaryBuffer;
    VkDeviceSize initialBufferSize = 0;
    
    // Performance tracking
    uint32_t totalAllocations = 0;
    uint32_t failedAllocations = 0;
};