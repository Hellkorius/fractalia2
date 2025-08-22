#include "spatial_map_buffers.h"
#include "../../vulkan/core/vulkan_context.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include "../../vulkan/core/vulkan_function_loader.h"
#include "../../vulkan/core/vulkan_utils.h"
#include <iostream>
#include <cstring>
#include <algorithm>

// SpatialMapBuffer implementation
SpatialMapBuffer::SpatialMapBuffer() {
}

SpatialMapBuffer::~SpatialMapBuffer() {
    cleanup();
}

bool SpatialMapBuffer::initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
    // Use BufferBase interface for initialization
    VkDeviceSize elementSize = SpatialMapConfig::CELL_DATA_SIZE * sizeof(uint32_t);
    uint32_t elementCount = SpatialMapConfig::TOTAL_CELLS;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    if (!BufferBase::initialize(context, resourceCoordinator, elementCount, elementSize, usage)) {
        std::cerr << "SpatialMapBuffer: Failed to initialize base buffer" << std::endl;
        return false;
    }
    
    std::cout << "SpatialMapBuffer: Created buffer with " << SpatialMapConfig::TOTAL_CELLS 
              << " cells (" << (getSize() / 1024 / 1024) << " MB)" << std::endl;
    
    return true;
}


bool SpatialMapBuffer::clearAllCells() {
    if (!isInitialized()) {
        return false;
    }
    
    // Create zero-initialized data (all cell counts = 0, all entity indices = 0xFFFFFFFF)
    std::vector<uint32_t> clearData(SpatialMapConfig::TOTAL_CELL_BUFFER_SIZE);
    
    for (uint32_t cellIndex = 0; cellIndex < SpatialMapConfig::TOTAL_CELLS; ++cellIndex) {
        uint32_t baseOffset = cellIndex * SpatialMapConfig::CELL_DATA_SIZE;
        clearData[baseOffset] = 0; // entity count = 0
        
        // Fill entity indices with invalid marker
        for (uint32_t i = 1; i < SpatialMapConfig::CELL_DATA_SIZE; ++i) {
            clearData[baseOffset + i] = 0xFFFFFFFF;
        }
    }
    
    return copyData(clearData.data(), clearData.size() * sizeof(uint32_t), 0);
}

// EntityCellBuffer implementation
EntityCellBuffer::EntityCellBuffer() {
}

EntityCellBuffer::~EntityCellBuffer() {
    cleanup();
}

bool EntityCellBuffer::initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
    // Use BufferBase interface for initialization
    VkDeviceSize elementSize = sizeof(uint32_t);  // Each entity maps to one cell index
    uint32_t elementCount = maxEntities;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    if (!BufferBase::initialize(context, resourceCoordinator, elementCount, elementSize, usage)) {
        std::cerr << "EntityCellBuffer: Failed to initialize base buffer" << std::endl;
        return false;
    }
    
    std::cout << "EntityCellBuffer: Created buffer for " << maxEntities 
              << " entities (" << (getSize() / 1024) << " KB)" << std::endl;
    
    return true;
}


// SpatialMapCoordinator implementation
SpatialMapCoordinator::SpatialMapCoordinator() {
}

SpatialMapCoordinator::~SpatialMapCoordinator() {
    cleanup();
}

bool SpatialMapCoordinator::initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
    this->maxEntities = maxEntities;
    
    // Initialize spatial map buffer
    if (!spatialMapBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "SpatialMapCoordinator: Failed to initialize spatial map buffer" << std::endl;
        return false;
    }
    
    // Initialize entity-to-cell mapping buffer
    if (!entityCellBuffer.initialize(context, resourceCoordinator, maxEntities)) {
        std::cerr << "SpatialMapCoordinator: Failed to initialize entity cell buffer" << std::endl;
        cleanup();
        return false;
    }
    
    // Clear spatial map on initialization
    if (!clearSpatialMap()) {
        std::cerr << "SpatialMapCoordinator: Failed to clear spatial map" << std::endl;
        cleanup();
        return false;
    }
    
    initialized = true;
    std::cout << "SpatialMapCoordinator: Initialized successfully" << std::endl;
    return true;
}

void SpatialMapCoordinator::cleanup() {
    spatialMapBuffer.cleanup();
    entityCellBuffer.cleanup();
    maxEntities = 0;
    initialized = false;
}

bool SpatialMapCoordinator::clearSpatialMap() {
    return spatialMapBuffer.clearAllCells();
}

bool SpatialMapCoordinator::uploadEntityCellData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return entityCellBuffer.copyData(data, size, offset);
}

// Utility functions for CPU-side calculations
uint32_t SpatialMapCoordinator::worldToCell(float worldX, float worldY) {
    // Convert world coordinates to grid coordinates
    glm::ivec2 gridCoords = worldToCellCoords(worldX, worldY);
    return cellCoordsToIndex(gridCoords.x, gridCoords.y);
}

glm::ivec2 SpatialMapCoordinator::worldToCellCoords(float worldX, float worldY) {
    // World extends from -WORLD_SIZE/2 to +WORLD_SIZE/2
    float halfWorld = SpatialMapConfig::WORLD_SIZE * 0.5f;
    
    // Normalize to [0, WORLD_SIZE]
    float normalizedX = worldX + halfWorld;
    float normalizedY = worldY + halfWorld;
    
    // Convert to grid coordinates [0, GRID_RESOLUTION-1]
    int32_t gridX = static_cast<int32_t>(normalizedX / SpatialMapConfig::CELL_SIZE);
    int32_t gridY = static_cast<int32_t>(normalizedY / SpatialMapConfig::CELL_SIZE);
    
    // Clamp to valid range
    gridX = std::clamp(gridX, 0, static_cast<int32_t>(SpatialMapConfig::GRID_RESOLUTION - 1));
    gridY = std::clamp(gridY, 0, static_cast<int32_t>(SpatialMapConfig::GRID_RESOLUTION - 1));
    
    return glm::ivec2(gridX, gridY);
}

glm::vec2 SpatialMapCoordinator::cellToWorld(uint32_t cellIndex) {
    uint32_t gridY = cellIndex / SpatialMapConfig::GRID_RESOLUTION;
    uint32_t gridX = cellIndex % SpatialMapConfig::GRID_RESOLUTION;
    
    float halfWorld = SpatialMapConfig::WORLD_SIZE * 0.5f;
    float cellSize = SpatialMapConfig::CELL_SIZE;
    
    // Convert grid coordinates to world coordinates (cell center)
    float worldX = (gridX * cellSize + cellSize * 0.5f) - halfWorld;
    float worldY = (gridY * cellSize + cellSize * 0.5f) - halfWorld;
    
    return glm::vec2(worldX, worldY);
}

uint32_t SpatialMapCoordinator::cellCoordsToIndex(uint32_t gridX, uint32_t gridY) {
    return gridY * SpatialMapConfig::GRID_RESOLUTION + gridX;
}