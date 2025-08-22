#include "entity_buffer_manager.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include "../../vulkan/resources/core/command_executor.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include <iostream>
#include <cstring>
#include <limits>
#include <algorithm>

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
    
    if (!spatialMapBuffer.initialize(context, resourceCoordinator, 4096)) { // 64x64 grid
        std::cerr << "EntityBufferManager: Failed to initialize spatial map buffer" << std::endl;
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
    spatialMapBuffer.cleanup();
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

bool EntityBufferManager::uploadSpatialMapData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return uploadService.upload(spatialMapBuffer, data, size, offset);
}

bool EntityBufferManager::uploadPositionDataToAllBuffers(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return positionCoordinator.uploadToAllBuffers(data, size, offset);
}

// Helper method to create staging buffer and read GPU data
bool EntityBufferManager::readGPUBuffer(VkBuffer srcBuffer, void* dstData, VkDeviceSize size, VkDeviceSize offset) const {
    // Access resourceCoordinator through uploadService
    auto* resourceCoordinator = uploadService.getResourceCoordinator();
    if (!resourceCoordinator) {
        return false;
    }
    
    const auto* context = resourceCoordinator->getContext();
    if (!context) {
        return false;
    }
    
    // Create staging buffer for readback
    auto stagingHandle = resourceCoordinator->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    if (!stagingHandle.buffer.get()) {
        std::cerr << "Failed to create staging buffer for readback" << std::endl;
        return false;
    }
    
    // Get command executor for the copy operation
    auto* commandExecutor = resourceCoordinator->getCommandExecutor();
    if (!commandExecutor) {
        resourceCoordinator->destroyResource(stagingHandle);
        return false;
    }
    
    // Use synchronous buffer copy (automatically handles command buffer creation/submission)
    commandExecutor->copyBufferToBuffer(srcBuffer, stagingHandle.buffer.get(), size, offset, 0);
    
    // Copy data from staging buffer to host memory
    if (stagingHandle.mappedData) {
        std::memcpy(dstData, stagingHandle.mappedData, size);
    } else {
        // Map, copy, unmap
        const auto& loader = context->getLoader();
        VkDevice device = context->getDevice();
        
        void* mappedData;
        VkResult result = loader.vkMapMemory(device, stagingHandle.memory.get(), 0, size, 0, &mappedData);
        if (result != VK_SUCCESS) {
            resourceCoordinator->destroyResource(stagingHandle);
            return false;
        }
        
        std::memcpy(dstData, mappedData, size);
        loader.vkUnmapMemory(device, stagingHandle.memory.get());
    }
    
    // Cleanup staging buffer
    resourceCoordinator->destroyResource(stagingHandle);
    return true;
}

// Debug readback implementations
bool EntityBufferManager::readbackEntityAtPosition(glm::vec2 worldPos, EntityDebugInfo& info) const {
    // Calculate spatial cell using same logic as GPU shader
    const float CELL_SIZE = 2.0f;
    const uint32_t GRID_WIDTH = 64;
    const uint32_t GRID_HEIGHT = 64;
    
    // Convert world position to grid coordinates (same as GPU)
    glm::ivec2 gridCoord = glm::ivec2(glm::floor(worldPos / CELL_SIZE));
    uint32_t x = static_cast<uint32_t>(gridCoord.x) & (GRID_WIDTH - 1);
    uint32_t y = static_cast<uint32_t>(gridCoord.y) & (GRID_HEIGHT - 1);
    uint32_t cellIndex = x + y * GRID_WIDTH;
    
    info.spatialCell = cellIndex;
    
    // Read spatial map from GPU to find entities in this cell
    std::vector<uint32_t> entitiesInCell;
    if (!readbackSpatialCell(cellIndex, entitiesInCell)) {
        std::cout << "Failed to read spatial cell " << cellIndex << std::endl;
        return false;
    }
    
    if (entitiesInCell.empty()) {
        std::cout << "No entities found in spatial cell " << cellIndex << " at world position (" 
                  << worldPos.x << ", " << worldPos.y << ")" << std::endl;
        return false;
    }
    
    // Find closest entity by reading position data for all entities in cell
    uint32_t closestEntity = entitiesInCell[0];
    float closestDistance = std::numeric_limits<float>::max();
    glm::vec4 closestPosition;
    
    for (uint32_t entityId : entitiesInCell) {
        if (entityId >= maxEntities) continue;
        
        // Read position for this entity
        glm::vec4 entityPosition;
        if (readGPUBuffer(positionCoordinator.getPrimaryBuffer(), 
                         &entityPosition, sizeof(glm::vec4), 
                         entityId * sizeof(glm::vec4))) {
            
            float distance = glm::distance(worldPos, glm::vec2(entityPosition));
            if (distance < closestDistance) {
                closestDistance = distance;
                closestEntity = entityId;
                closestPosition = entityPosition;
            }
        }
    }
    
    info.entityId = closestEntity;
    info.position = closestPosition;
    
    // Read velocity for the closest entity
    if (!readGPUBuffer(velocityBuffer.getBuffer(), 
                      &info.velocity, sizeof(glm::vec4), 
                      closestEntity * sizeof(glm::vec4))) {
        info.velocity = glm::vec4(0.0f); // Fallback
    }
    
    return true;
}

bool EntityBufferManager::readbackEntityById(uint32_t entityId, EntityDebugInfo& info) const {
    if (entityId >= maxEntities) {
        return false;
    }
    
    info.entityId = entityId;
    
    // Read position
    if (!readGPUBuffer(positionCoordinator.getPrimaryBuffer(), 
                      &info.position, sizeof(glm::vec4), 
                      entityId * sizeof(glm::vec4))) {
        return false;
    }
    
    // Read velocity
    if (!readGPUBuffer(velocityBuffer.getBuffer(), 
                      &info.velocity, sizeof(glm::vec4), 
                      entityId * sizeof(glm::vec4))) {
        info.velocity = glm::vec4(0.0f);
    }
    
    // Calculate spatial cell from position (same logic as GPU)
    const float CELL_SIZE = 2.0f;
    const uint32_t GRID_WIDTH = 64;
    
    glm::vec2 pos2D = glm::vec2(info.position);
    glm::ivec2 gridCoord = glm::ivec2(glm::floor(pos2D / CELL_SIZE));
    uint32_t x = static_cast<uint32_t>(gridCoord.x) & (GRID_WIDTH - 1);
    uint32_t y = static_cast<uint32_t>(gridCoord.y) & (GRID_WIDTH - 1);
    info.spatialCell = x + y * GRID_WIDTH;
    
    return true;
}

bool EntityBufferManager::readbackSpatialCell(uint32_t cellIndex, std::vector<uint32_t>& entityIds) const {
    const uint32_t SPATIAL_MAP_SIZE = 4096; // 64x64
    if (cellIndex >= SPATIAL_MAP_SIZE) {
        return false;
    }
    
    entityIds.clear();
    
    // Read the spatial cell entry (uvec2: entityId, nextIndex)
    glm::uvec2 cellData;
    if (!readGPUBuffer(spatialMapBuffer.getBuffer(), 
                      &cellData, sizeof(glm::uvec2), 
                      cellIndex * sizeof(glm::uvec2))) {
        return false;
    }
    
    // Walk the linked list to collect all entities in this cell
    const uint32_t NULL_INDEX = 0xFFFFFFFF;
    uint32_t currentEntity = cellData.x;
    
    // Safety: limit iterations to prevent infinite loops
    uint32_t iterations = 0;
    const uint32_t MAX_ITERATIONS = 100;
    
    while (currentEntity != NULL_INDEX && currentEntity < maxEntities && iterations < MAX_ITERATIONS) {
        entityIds.push_back(currentEntity);
        
        // Read next link in the chain - the spatial map stores links differently
        // Each entity has its "next" pointer stored in the spatial map at its entity index
        glm::uvec2 nextCellData;
        if (!readGPUBuffer(spatialMapBuffer.getBuffer(), 
                          &nextCellData, sizeof(glm::uvec2), 
                          currentEntity * sizeof(glm::uvec2))) {
            break;
        }
        
        currentEntity = nextCellData.y; // Next pointer is in .y component
        iterations++;
    }
    
    return true;
}

