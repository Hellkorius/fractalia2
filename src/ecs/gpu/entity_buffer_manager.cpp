#include "entity_buffer_manager.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include <iostream>
#include <cstring>

EntityBufferManager::EntityBufferManager() {
}

EntityBufferManager::~EntityBufferManager() {
    cleanup();
}

bool EntityBufferManager::initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
    this->maxEntities = maxEntities;
    
    // Initialize upload service
    if (!uploadService.initialize(resourceCoordinator)) {
        std::cerr << "EntityBufferManager: Failed to initialize upload service" << std::endl;
        return false;
    }
    
    // Initialize specialized buffers
    if (!velocityBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "EntityBufferManager: Failed to initialize velocity buffer" << std::endl;
        return false;
    }
    
    if (!movementParamsBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "EntityBufferManager: Failed to initialize movement params buffer" << std::endl;
        return false;
    }
    
    if (!runtimeStateBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "EntityBufferManager: Failed to initialize runtime state buffer" << std::endl;
        return false;
    }
    
    if (!colorBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "EntityBufferManager: Failed to initialize color buffer" << std::endl;
        return false;
    }
    
    if (!modelMatrixBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "EntityBufferManager: Failed to initialize model matrix buffer" << std::endl;
        return false;
    }
    
    // Initialize position buffer coordinator
    if (!positionCoordinator.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "EntityBufferManager: Failed to initialize position coordinator" << std::endl;
        return false;
    }
    
    std::cout << "EntityBufferManager: Initialized successfully for " << maxEntities << " entities using SRP-compliant design" << std::endl;
    return true;
}

void EntityBufferManager::cleanup() {
    // Cleanup specialized components
    positionCoordinator.cleanup();
    modelMatrixBuffer.cleanup();
    colorBuffer.cleanup();
    runtimeStateBuffer.cleanup();
    movementParamsBuffer.cleanup();
    velocityBuffer.cleanup();
    uploadService.cleanup();
    
    maxEntities = 0;
}


bool EntityBufferManager::uploadVelocityData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return uploadService.upload(velocityBuffer, data, size, offset);
}

bool EntityBufferManager::uploadMovementParamsData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return uploadService.upload(movementParamsBuffer, data, size, offset);
}

bool EntityBufferManager::uploadRuntimeStateData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return uploadService.upload(runtimeStateBuffer, data, size, offset);
}

bool EntityBufferManager::uploadColorData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return uploadService.upload(colorBuffer, data, size, offset);
}

bool EntityBufferManager::uploadModelMatrixData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return uploadService.upload(modelMatrixBuffer, data, size, offset);
}

bool EntityBufferManager::uploadPositionDataToAllBuffers(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return positionCoordinator.uploadToAllBuffers(data, size, offset);
}

