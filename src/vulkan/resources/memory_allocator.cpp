#include "memory_allocator.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include <iostream>
#include <stdexcept>

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
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    if (context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &allocation.memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory!" << std::endl;
        return {};
    }
    
    allocation.size = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    
    // Update stats
    memoryStats.totalAllocated += requirements.size;
    memoryStats.activeAllocations++;
    
    return allocation;
}

void MemoryAllocator::freeMemory(const AllocationInfo& allocation) {
    if (!context || allocation.memory == VK_NULL_HANDLE) return;
    
    if (allocation.mappedData) {
        context->getLoader().vkUnmapMemory(context->getDevice(), allocation.memory);
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

uint32_t MemoryAllocator::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    context->getLoader().vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

bool MemoryAllocator::initializeVMA() {
    // Simplified VMA initialization - just store device info
    allocator = new VmaAllocator_Impl;
    allocator->device = context->getDevice();
    allocator->physicalDevice = context->getPhysicalDevice();
    allocator->loader = &context->getLoader();
    
    return true;
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