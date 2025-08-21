#include "entity_buffer_manager.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/resources/managers/resource_context.h"
#include "../../vulkan/resources/core/resource_handle.h"
#include "../../vulkan/core/vulkan_raii.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include "../../vulkan/core/vulkan_utils.h"
#include "../components/component.h"
#include "gpu_entity_manager.h"  // For GPUEntity definition
#include <iostream>
#include <cstring>

EntityBufferManager::EntityBufferManager() {
}

EntityBufferManager::~EntityBufferManager() {
    cleanup();
}

bool EntityBufferManager::initialize(const VulkanContext& context, ResourceContext* resourceContext, uint32_t maxEntities) {
    this->context = &context;
    this->resourceContext = resourceContext;
    this->maxEntities = maxEntities;
    
    // Calculate SoA buffer sizes
    velocityBufferSize = maxEntities * sizeof(glm::vec4);
    movementParamsBufferSize = maxEntities * sizeof(glm::vec4);
    runtimeStateBufferSize = maxEntities * sizeof(glm::vec4);
    colorBufferSize = maxEntities * sizeof(glm::vec4);
    modelMatrixBufferSize = maxEntities * sizeof(glm::mat4);
    positionBufferSize = maxEntities * sizeof(glm::vec4);
    
    // Create SoA buffers
    const VkBufferUsageFlags soaUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    if (!createBuffer(velocityBufferSize, soaUsage, velocityBuffer, velocityBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create velocity buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(movementParamsBufferSize, soaUsage, movementParamsBuffer, movementParamsBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create movement params buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(runtimeStateBufferSize, soaUsage, runtimeStateBuffer, runtimeStateBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create runtime state buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(colorBufferSize, soaUsage, colorBuffer, colorBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create color buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(modelMatrixBufferSize, soaUsage, modelMatrixBuffer, modelMatrixBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create model matrix buffer" << std::endl;
        return false;
    }
    
    // Create position buffers
    if (!createBuffer(positionBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     positionBuffer, positionBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create position buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(positionBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     positionBufferAlternate, positionBufferAlternateMemory)) {
        std::cerr << "EntityBufferManager: Failed to create alternate position buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(positionBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     currentPositionBuffer, currentPositionBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create current position buffer" << std::endl;
        return false;
    }
    
    if (!createBuffer(positionBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     targetPositionBuffer, targetPositionBufferMemory)) {
        std::cerr << "EntityBufferManager: Failed to create target position buffer" << std::endl;
        return false;
    }
    
    std::cout << "EntityBufferManager: Initialized successfully for " << maxEntities << " entities" << std::endl;
    return true;
}

void EntityBufferManager::cleanup() {
    if (!context) return;
    
    // Cleanup position buffers
    destroyBuffer(targetPositionBuffer, targetPositionBufferMemory);
    destroyBuffer(currentPositionBuffer, currentPositionBufferMemory);
    destroyBuffer(positionBufferAlternate, positionBufferAlternateMemory);
    destroyBuffer(positionBuffer, positionBufferMemory);
    
    // Cleanup SoA buffers
    destroyBuffer(modelMatrixBuffer, modelMatrixBufferMemory);
    destroyBuffer(colorBuffer, colorBufferMemory);
    destroyBuffer(runtimeStateBuffer, runtimeStateBufferMemory);
    destroyBuffer(movementParamsBuffer, movementParamsBufferMemory);
    destroyBuffer(velocityBuffer, velocityBufferMemory);
    
    context = nullptr;
    resourceContext = nullptr;
}

VkBuffer EntityBufferManager::getComputeWriteBuffer(uint32_t frameIndex) const {
    // Compute writes to different buffer each frame (ping-pong)
    return (frameIndex % 2 == 0) ? positionBuffer : positionBufferAlternate;
}

VkBuffer EntityBufferManager::getGraphicsReadBuffer(uint32_t frameIndex) const {
    // SIMPLE FIX: On frame 0, read from same buffer compute writes to (no "previous" frame)
    // This prevents reading garbage data on the very first frame
    if (frameIndex == 0) {
        return getComputeWriteBuffer(0); // positionBuffer for frame 0
    }
    // Normal ping-pong: graphics reads from previous frame's compute output  
    return (frameIndex % 2 == 0) ? positionBufferAlternate : positionBuffer;
}

void EntityBufferManager::copyDataToBuffer(VkBuffer buffer, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    // Create temporary ResourceHandle for existing buffer
    ResourceHandle handle{};
    handle.buffer = vulkan_raii::make_buffer(buffer, context);
    
    // Determine full buffer size based on which buffer is being used
    if (buffer == velocityBuffer) {
        handle.size = velocityBufferSize;
    } else if (buffer == movementParamsBuffer) {
        handle.size = movementParamsBufferSize;
    } else if (buffer == runtimeStateBuffer) {
        handle.size = runtimeStateBufferSize;
    } else if (buffer == colorBuffer) {
        handle.size = colorBufferSize;
    } else if (buffer == modelMatrixBuffer) {
        handle.size = modelMatrixBufferSize;
    } else {
        // Position buffers or other buffers
        handle.size = positionBufferSize;
    }
    
    // Use ResourceContext's built-in staging infrastructure with offset
    resourceContext->copyToBuffer(handle, data, size, offset);
    
    // Detach to prevent cleanup of existing buffer
    handle.buffer.detach();
}

bool EntityBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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
        return false;
    }
    
    vk.vkBindBufferMemory(device, buffer, bufferMemory, 0);
    return true;
}

void EntityBufferManager::destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    if (!context) return;
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    if (buffer != VK_NULL_HANDLE) {
        vk.vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    
    if (memory != VK_NULL_HANDLE) {
        vk.vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}