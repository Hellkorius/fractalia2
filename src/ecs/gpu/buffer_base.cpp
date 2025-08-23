#include "buffer_base.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include "../../vulkan/core/vulkan_utils.h"
#include "../../vulkan/resources/core/resource_handle.h"
#include "../../vulkan/core/vulkan_raii.h"
#include <iostream>

BufferBase::BufferBase() {
}

BufferBase::~BufferBase() {
    cleanup();
}

bool BufferBase::initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, 
                           uint32_t maxElements, VkDeviceSize elementSize, VkBufferUsageFlags usage) {
    this->context = &context;
    this->resourceCoordinator = resourceCoordinator;
    this->maxElements = maxElements;
    this->elementSize = elementSize;
    this->bufferSize = maxElements * elementSize;
    
    // Standard buffer usage for entity data
    const VkBufferUsageFlags standardUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    // Allow subclasses to add specific usage flags
    const VkBufferUsageFlags finalUsage = standardUsage | usage | getAdditionalUsageFlags();
    
    if (!createBuffer(bufferSize, finalUsage)) {
        std::cerr << "BufferBase: Failed to create " << getBufferTypeName() << " buffer" << std::endl;
        return false;
    }
    
    std::cout << "BufferBase: Initialized " << getBufferTypeName() << " buffer for " 
              << maxElements << " elements (" << bufferSize << " bytes)" << std::endl;
    return true;
}

void BufferBase::cleanup() {
    destroyBuffer();
    context = nullptr;
    resourceCoordinator = nullptr;
    maxElements = 0;
    elementSize = 0;
    bufferSize = 0;
}

bool BufferBase::copyData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!isInitialized() || !resourceCoordinator) {
        std::cerr << "BufferBase: Cannot copy data - " << getBufferTypeName() << " buffer not initialized" << std::endl;
        return false;
    }
    
    if (offset + size > bufferSize) {
        std::cerr << "BufferBase: Copy would exceed " << getBufferTypeName() << " buffer size" << std::endl;
        return false;
    }
    
    // Create temporary ResourceHandle for existing buffer
    ResourceHandle handle{};
    handle.buffer = vulkan_raii::make_buffer(buffer, context);
    handle.size = bufferSize;
    
    // Use ResourceCoordinator's staging infrastructure
    bool success = resourceCoordinator->copyToBuffer(handle, data, size, offset);
    
    // Detach to prevent cleanup of existing buffer
    handle.buffer.detach();
    
    return success;
}

bool BufferBase::readData(void* data, VkDeviceSize size, VkDeviceSize offset) const {
    if (!isInitialized()) {
        std::cerr << "BufferBase: Cannot read data - " << getBufferTypeName() << " buffer not initialized" << std::endl;
        return false;
    }
    
    if (offset + size > bufferSize) {
        std::cerr << "BufferBase: Read would exceed " << getBufferTypeName() << " buffer size" << std::endl;
        return false;
    }
    
    // For debug readback, we need to implement a simple staging buffer approach
    // This is expensive and should only be used for debugging
    std::cerr << "BufferBase::readData: GPU readback not implemented yet - spatial map data not available" << std::endl;
    return false;
}

bool BufferBase::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    if (vk.vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vk.vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(
        context->getPhysicalDevice(), vk, memRequirements.memoryTypeBits, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vk.vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vk.vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }
    
    vk.vkBindBufferMemory(device, buffer, bufferMemory, 0);
    return true;
}

void BufferBase::destroyBuffer() {
    if (!context) return;
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    if (buffer != VK_NULL_HANDLE) {
        vk.vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    
    if (bufferMemory != VK_NULL_HANDLE) {
        vk.vkFreeMemory(device, bufferMemory, nullptr);
        bufferMemory = VK_NULL_HANDLE;
    }
}