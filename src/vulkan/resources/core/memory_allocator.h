#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

class VulkanContext;

// VMA allocator wrapper - simplified interface without external dependency
class VmaAllocator_Impl;
using VmaAllocator = VmaAllocator_Impl*;
using VmaAllocation = void*;

// Memory allocation and management
class MemoryAllocator {
public:
    MemoryAllocator();
    ~MemoryAllocator();
    
    bool initialize(const VulkanContext& context);
    void cleanup();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Raw memory allocation
    struct AllocationInfo {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkDeviceSize offset = 0;
        void* mappedData = nullptr;
        uint32_t memoryTypeIndex = 0;
    };
    
    AllocationInfo allocateMemory(VkMemoryRequirements requirements, 
                                  VkMemoryPropertyFlags properties);
    void freeMemory(const AllocationInfo& allocation);
    
    // Memory mapping - centralized for all resource types
    bool mapMemory(const AllocationInfo& allocation, void** data);
    void unmapMemory(const AllocationInfo& allocation);
    
    // Resource handle memory mapping (centralized to eliminate duplication)
    bool mapResourceMemory(class ResourceHandle& handle);
    void unmapResourceMemory(class ResourceHandle& handle);
    
    // Utility for creating pre-mapped allocations
    AllocationInfo allocateMappedMemory(VkMemoryRequirements requirements,
                                       VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    // Memory type utilities
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Memory pressure detection and management
    struct DeviceMemoryBudget {
        VkDeviceSize heapSize = 0;
        VkDeviceSize usedBytes = 0;
        VkDeviceSize availableBytes = 0;
        float pressureRatio = 0.0f; // 0.0 = no pressure, 1.0 = critical
    };
    
    bool isUnderMemoryPressure() const;
    DeviceMemoryBudget getMemoryBudget(uint32_t heapIndex) const;
    bool attemptMemoryRecovery();
    
    // Statistics with pressure tracking
    struct MemoryStats {
        VkDeviceSize totalAllocated = 0;
        VkDeviceSize totalFreed = 0;
        uint32_t activeAllocations = 0;
        VkDeviceSize peakUsage = 0;
        uint32_t failedAllocations = 0;
        bool memoryPressure = false;
        float fragmentationRatio = 0.0f;
    };
    
    MemoryStats getMemoryStats() const { return memoryStats; }

private:
    const VulkanContext* context = nullptr;
    VmaAllocator allocator = nullptr;
    MemoryStats memoryStats;
    
    // Internal VMA wrapper functions
    bool initializeVMA();
    void cleanupVMA();
};