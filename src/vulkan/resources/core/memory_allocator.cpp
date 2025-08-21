#include "memory_allocator.h"
#include "resource_handle.h"
#include "validation_utils.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

// Simple VMA allocator replacement using manual allocation
struct VmaAllocator_Impl {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    const VulkanFunctionLoader* loader;
    
    struct Allocation {
        VkDeviceMemory memory;
        VkDeviceSize offset;
        VkDeviceSize size;
        void* mappedData;
    };
    
    std::vector<Allocation> allocations;
};

MemoryAllocator::MemoryAllocator() {
}

MemoryAllocator::~MemoryAllocator() {
    cleanup();
}

bool MemoryAllocator::initialize(const VulkanContext& context) {
    this->context = &context;
    
    if (!initializeVMA()) {
        return false;
    }
    
    return true;
}

void MemoryAllocator::cleanup() {
    cleanupVMA();
    context = nullptr;
}

MemoryAllocator::AllocationInfo MemoryAllocator::allocateMemory(VkMemoryRequirements requirements, 
                                                                VkMemoryPropertyFlags properties) {
    AllocationInfo allocation;
    
    uint32_t memoryType = findMemoryType(requirements.memoryTypeBits, properties);
    
    // Check memory pressure before allocation
    if (isUnderMemoryPressure()) {
        std::cerr << "Warning: GPU under memory pressure, attempting recovery..." << std::endl;
        if (!attemptMemoryRecovery()) {
            std::cerr << "Memory recovery failed, proceeding with risky allocation" << std::endl;
        }
    }
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    VkResult result = context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &allocation.memory);
    if (result != VK_SUCCESS) {
        memoryStats.failedAllocations++;
        
        if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY) {
            std::cerr << "Out of memory - attempting emergency recovery..." << std::endl;
            if (attemptMemoryRecovery()) {
                // Retry allocation after recovery
                result = context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &allocation.memory);
            }
        }
        
        if (result != VK_SUCCESS) {
            std::cerr << "Critical: Memory allocation failed after recovery attempts!" << std::endl;
            return {};
        }
    }
    
    allocation.size = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    
    // Track allocation in VMA wrapper for proper cleanup
    if (allocator) {
        VmaAllocator_Impl::Allocation vmaAlloc;
        vmaAlloc.memory = allocation.memory;
        vmaAlloc.offset = 0;
        vmaAlloc.size = requirements.size;
        vmaAlloc.mappedData = nullptr;
        allocator->allocations.push_back(vmaAlloc);
    }
    
    // Update comprehensive stats
    memoryStats.totalAllocated += requirements.size;
    memoryStats.activeAllocations++;
    
    if (memoryStats.totalAllocated - memoryStats.totalFreed > memoryStats.peakUsage) {
        memoryStats.peakUsage = memoryStats.totalAllocated - memoryStats.totalFreed;
    }
    
    // Update pressure status
    memoryStats.memoryPressure = isUnderMemoryPressure();
    
    return allocation;
}

void MemoryAllocator::freeMemory(const AllocationInfo& allocation) {
    if (!context || allocation.memory == VK_NULL_HANDLE) return;
    
    if (allocation.mappedData) {
        context->getLoader().vkUnmapMemory(context->getDevice(), allocation.memory);
    }
    
    // Remove from VMA tracking before freeing
    if (allocator) {
        auto& allocations = allocator->allocations;
        allocations.erase(
            std::remove_if(allocations.begin(), allocations.end(),
                [&allocation](const VmaAllocator_Impl::Allocation& alloc) {
                    return alloc.memory == allocation.memory;
                }),
            allocations.end()
        );
    }
    
    context->getLoader().vkFreeMemory(context->getDevice(), allocation.memory, nullptr);
    
    // Update stats
    memoryStats.totalFreed += allocation.size;
    memoryStats.activeAllocations--;
}

bool MemoryAllocator::mapMemory(const AllocationInfo& allocation, void** data) {
    if (!context || allocation.memory == VK_NULL_HANDLE) return false;
    
    if (context->getLoader().vkMapMemory(context->getDevice(), allocation.memory, 
                                         allocation.offset, allocation.size, 0, data) != VK_SUCCESS) {
        std::cerr << "Failed to map memory!" << std::endl;
        return false;
    }
    
    return true;
}

void MemoryAllocator::unmapMemory(const AllocationInfo& allocation) {
    if (context && allocation.memory != VK_NULL_HANDLE) {
        context->getLoader().vkUnmapMemory(context->getDevice(), allocation.memory);
    }
}

bool MemoryAllocator::mapResourceMemory(ResourceHandle& handle) {
    if (!ValidationUtils::validateDependencies("MemoryAllocator::mapResourceMemory", context, &handle)) {
        return false;
    }
    
    if (!handle.isValid()) {
        ValidationUtils::logValidationFailure("MemoryAllocator::mapResourceMemory", 
                                             "resource handle", "invalid handle");
        return false;
    }
    
    if (handle.mappedData) {
        // Already mapped
        return true;
    }
    
    if (!handle.memory.get()) {
        ValidationUtils::logValidationFailure("MemoryAllocator::mapResourceMemory",
                                             "resource memory", "null memory handle");
        return false;
    }
    
    AllocationInfo allocation;
    allocation.memory = handle.memory.get();
    allocation.size = handle.size;
    allocation.offset = 0; // Assuming full resource mapping
    
    return mapMemory(allocation, &handle.mappedData);
}

void MemoryAllocator::unmapResourceMemory(ResourceHandle& handle) {
    if (!context || !handle.isValid() || !handle.mappedData) {
        return;
    }
    
    if (handle.memory.get()) {
        context->getLoader().vkUnmapMemory(context->getDevice(), handle.memory.get());
        handle.mappedData = nullptr;
    }
}

MemoryAllocator::AllocationInfo MemoryAllocator::allocateMappedMemory(VkMemoryRequirements requirements,
                                                                     VkMemoryPropertyFlags properties) {
    AllocationInfo allocation = allocateMemory(requirements, properties);
    
    if (allocation.memory != VK_NULL_HANDLE && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        if (!mapMemory(allocation, &allocation.mappedData)) {
            ValidationUtils::logError("MemoryAllocator", "allocateMappedMemory", 
                                     "failed to map allocated memory");
            freeMemory(allocation);
            return {};
        }
    }
    
    return allocation;
}

uint32_t MemoryAllocator::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    context->getLoader().vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);
    
    // First pass: exact match
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    // Second pass: fallback to compatible memory type with required properties
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) != 0) {
            std::cerr << "Warning: Using fallback memory type " << i 
                      << " (requested properties not fully supported)" << std::endl;
            return i;
        }
    }
    
    // Final fallback: any valid memory type from filter
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            std::cerr << "Warning: Using basic fallback memory type " << i 
                      << " (properties may not match requirements)" << std::endl;
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find any suitable memory type!");
}

bool MemoryAllocator::initializeVMA() {
    // Simplified VMA initialization - just store device info
    allocator = new VmaAllocator_Impl;
    allocator->device = context->getDevice();
    allocator->physicalDevice = context->getPhysicalDevice();
    allocator->loader = &context->getLoader();
    
    return true;
}

bool MemoryAllocator::isUnderMemoryPressure() const {
    if (!context || !allocator) return false;
    
    VkPhysicalDeviceMemoryProperties memProps;
    context->getLoader().vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProps);
    
    // Check each heap for pressure (>80% usage indicates pressure)
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        DeviceMemoryBudget budget = getMemoryBudget(i);
        if (budget.pressureRatio > 0.8f) {
            return true;
        }
    }
    
    return false;
}

MemoryAllocator::DeviceMemoryBudget MemoryAllocator::getMemoryBudget(uint32_t heapIndex) const {
    DeviceMemoryBudget budget{};
    
    if (!context) return budget;
    
    VkPhysicalDeviceMemoryProperties memProps;
    context->getLoader().vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProps);
    
    if (heapIndex >= memProps.memoryHeapCount) return budget;
    
    budget.heapSize = memProps.memoryHeaps[heapIndex].size;
    
    // Estimate used bytes from our allocations (simplified)
    VkDeviceSize usedInHeap = 0;
    for (const auto& alloc : allocator->allocations) {
        // Simple heuristic: assume allocation is in this heap if it's device-local
        usedInHeap += alloc.size;
    }
    
    budget.usedBytes = usedInHeap;
    budget.availableBytes = budget.heapSize > budget.usedBytes ? budget.heapSize - budget.usedBytes : 0;
    budget.pressureRatio = budget.heapSize > 0 ? (float)budget.usedBytes / budget.heapSize : 1.0f;
    
    return budget;
}

bool MemoryAllocator::attemptMemoryRecovery() {
    if (!allocator) return false;
    
    // Force garbage collection by triggering a memory defragmentation-like cleanup
    size_t recoveredBytes = 0;
    
    // In a real VMA implementation, this would trigger defragmentation
    // For now, we'll just log the attempt
    std::cout << "Attempting memory recovery - would trigger VMA defragmentation here" << std::endl;
    
    return recoveredBytes > 0;
}

void MemoryAllocator::cleanupVMA() {
    if (allocator) {
        // Clean up any remaining allocations
        for (auto& alloc : allocator->allocations) {
            if (alloc.mappedData) {
                allocator->loader->vkUnmapMemory(allocator->device, alloc.memory);
            }
            allocator->loader->vkFreeMemory(allocator->device, alloc.memory, nullptr);
        }
        
        delete allocator;
        allocator = nullptr;
    }
}