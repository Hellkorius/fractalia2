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
    
    // Memory mapping
    bool mapMemory(const AllocationInfo& allocation, void** data);
    void unmapMemory(const AllocationInfo& allocation);
    
    // Memory type utilities
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Statistics
    struct MemoryStats {
        VkDeviceSize totalAllocated = 0;
        VkDeviceSize totalFreed = 0;
        uint32_t activeAllocations = 0;
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