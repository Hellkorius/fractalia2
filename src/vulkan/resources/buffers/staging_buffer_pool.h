#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../core/resource_handle.h"

class VulkanContext;

class StagingBufferPool {
public:
    StagingBufferPool() = default;
    ~StagingBufferPool();
    
    bool initialize(const VulkanContext& context, VkDeviceSize size);
    void cleanup();
    
    struct StagingRegion {
        void* mappedData;
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
        
        bool isValid() const { return mappedData != nullptr && buffer != VK_NULL_HANDLE; }
    };
    
    class StagingRegionGuard {
    public:
        StagingRegionGuard(StagingBufferPool* pool, VkDeviceSize size, VkDeviceSize alignment = 1)
            : stagingPool(pool), region(pool ? pool->allocate(size, alignment) : StagingRegion{}) {}
        
        ~StagingRegionGuard() = default;
        
        StagingRegionGuard(const StagingRegionGuard&) = delete;
        StagingRegionGuard& operator=(const StagingRegionGuard&) = delete;
        
        StagingRegionGuard(StagingRegionGuard&& other) noexcept 
            : stagingPool(other.stagingPool), region(other.region) {
            other.stagingPool = nullptr;
            other.region = {};
        }
        
        StagingRegionGuard& operator=(StagingRegionGuard&& other) noexcept {
            if (this != &other) {
                stagingPool = other.stagingPool;
                region = other.region;
                other.stagingPool = nullptr;
                other.region = {};
            }
            return *this;
        }
        
        const StagingRegion& get() const { return region; }
        bool isValid() const { return region.isValid(); }
        
        const StagingRegion* operator->() const { return &region; }
        const StagingRegion& operator*() const { return region; }
        
    private:
        StagingBufferPool* stagingPool;
        StagingRegion region;
    };
    
    StagingRegion allocate(VkDeviceSize size, VkDeviceSize alignment = 1);
    StagingRegionGuard allocateGuarded(VkDeviceSize size, VkDeviceSize alignment = 1);
    void reset();
    
    bool tryDefragment();
    VkDeviceSize getFragmentedBytes() const;
    bool isFragmentationCritical() const;
    
    VkBuffer getBuffer() const { return ringBuffer.buffer.get(); }
    VkDeviceSize getTotalSize() const { return totalSize; }
    
    struct PoolStats {
        VkDeviceSize totalSize = 0;
        VkDeviceSize fragmentedBytes = 0;
        float fragmentationRatio = 0.0f;
        bool fragmentationCritical = false;
        uint32_t allocations = 0;
        uint32_t failedAllocations = 0;
    };
    
    PoolStats getStats() const;
    bool isUnderPressure() const;
    
private:
    const VulkanContext* context = nullptr;
    ResourceHandle ringBuffer;
    VkDeviceSize currentOffset = 0;
    VkDeviceSize totalSize = 0;
    
    VkDeviceSize totalWastedBytes = 0;
    uint32_t wrapAroundCount = 0;
    VkDeviceSize largestFreeBlock = 0;
    
    mutable uint32_t totalAllocations = 0;
    mutable uint32_t failedAllocations = 0;
};