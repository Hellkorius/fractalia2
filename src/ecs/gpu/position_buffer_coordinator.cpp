#include "position_buffer_coordinator.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/resources/managers/resource_context.h"
#include <iostream>

PositionBufferCoordinator::PositionBufferCoordinator() {
}

PositionBufferCoordinator::~PositionBufferCoordinator() {
    cleanup();
}

bool PositionBufferCoordinator::initialize(const VulkanContext& context, ResourceContext* resourceContext, uint32_t maxEntities) {
    this->maxEntities = maxEntities;
    
    // Initialize all position buffers
    if (!primaryBuffer.initialize(context, resourceContext, maxEntities)) {
        std::cerr << "PositionBufferCoordinator: Failed to initialize primary buffer" << std::endl;
        return false;
    }
    
    if (!alternateBuffer.initialize(context, resourceContext, maxEntities)) {
        std::cerr << "PositionBufferCoordinator: Failed to initialize alternate buffer" << std::endl;
        return false;
    }
    
    if (!currentBuffer.initialize(context, resourceContext, maxEntities)) {
        std::cerr << "PositionBufferCoordinator: Failed to initialize current buffer" << std::endl;
        return false;
    }
    
    if (!targetBuffer.initialize(context, resourceContext, maxEntities)) {
        std::cerr << "PositionBufferCoordinator: Failed to initialize target buffer" << std::endl;
        return false;
    }
    
    std::cout << "PositionBufferCoordinator: Initialized successfully for " << maxEntities << " entities" << std::endl;
    return true;
}

void PositionBufferCoordinator::cleanup() {
    targetBuffer.cleanup();
    currentBuffer.cleanup();
    alternateBuffer.cleanup();
    primaryBuffer.cleanup();
    maxEntities = 0;
}

VkBuffer PositionBufferCoordinator::getComputeWriteBuffer(uint32_t frameIndex) const {
    // Compute writes to different buffer each frame (ping-pong)
    return (frameIndex % 2 == 0) ? primaryBuffer.getBuffer() : alternateBuffer.getBuffer();
}

VkBuffer PositionBufferCoordinator::getGraphicsReadBuffer(uint32_t frameIndex) const {
    // SIMPLE FIX: On frame 0, read from same buffer compute writes to (no "previous" frame)
    // This prevents reading garbage data on the very first frame
    if (frameIndex == 0) {
        return getComputeWriteBuffer(0); // primaryBuffer for frame 0
    }
    // Normal ping-pong: graphics reads from previous frame's compute output  
    return (frameIndex % 2 == 0) ? alternateBuffer.getBuffer() : primaryBuffer.getBuffer();
}

bool PositionBufferCoordinator::uploadToAllBuffers(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!isInitialized()) {
        std::cerr << "PositionBufferCoordinator: Not initialized" << std::endl;
        return false;
    }
    
    bool allSucceeded = true;
    
    if (!primaryBuffer.copyData(data, size, offset)) {
        std::cerr << "PositionBufferCoordinator: Failed to upload to primary buffer" << std::endl;
        allSucceeded = false;
    }
    
    if (!alternateBuffer.copyData(data, size, offset)) {
        std::cerr << "PositionBufferCoordinator: Failed to upload to alternate buffer" << std::endl;
        allSucceeded = false;
    }
    
    if (!currentBuffer.copyData(data, size, offset)) {
        std::cerr << "PositionBufferCoordinator: Failed to upload to current buffer" << std::endl;
        allSucceeded = false;
    }
    
    if (!targetBuffer.copyData(data, size, offset)) {
        std::cerr << "PositionBufferCoordinator: Failed to upload to target buffer" << std::endl;
        allSucceeded = false;
    }
    
    return allSucceeded;
}

bool PositionBufferCoordinator::uploadToPrimary(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return primaryBuffer.copyData(data, size, offset);
}

bool PositionBufferCoordinator::uploadToAlternate(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return alternateBuffer.copyData(data, size, offset);
}

bool PositionBufferCoordinator::uploadToCurrent(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return currentBuffer.copyData(data, size, offset);
}

bool PositionBufferCoordinator::uploadToTarget(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return targetBuffer.copyData(data, size, offset);
}

bool PositionBufferCoordinator::isInitialized() const {
    return primaryBuffer.isInitialized() && 
           alternateBuffer.isInitialized() && 
           currentBuffer.isInitialized() && 
           targetBuffer.isInitialized();
}