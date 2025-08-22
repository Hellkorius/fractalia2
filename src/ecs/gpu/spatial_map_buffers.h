#pragma once

#include "specialized_buffers.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// Forward declarations
class VulkanContext;
class ResourceCoordinator;

/**
 * Spatial map configuration constants
 */
struct SpatialMapConfig {
    static constexpr float WORLD_SIZE = 2000.0f;           // World extends from -1000 to +1000 in X and Y
    static constexpr uint32_t GRID_RESOLUTION = 128;       // 128x128 grid = 16,384 cells
    static constexpr float CELL_SIZE = WORLD_SIZE / GRID_RESOLUTION; // ~15.625 units per cell
    static constexpr uint32_t MAX_ENTITIES_PER_CELL = 64;  // Maximum entities that can be stored per cell
    static constexpr uint32_t TOTAL_CELLS = GRID_RESOLUTION * GRID_RESOLUTION;
    
    // GPU buffer sizing
    static constexpr uint32_t CELL_DATA_SIZE = MAX_ENTITIES_PER_CELL + 1; // +1 for count
    static constexpr uint32_t TOTAL_CELL_BUFFER_SIZE = TOTAL_CELLS * CELL_DATA_SIZE;
};

/**
 * GPU spatial map cell structure
 * Layout: [count, entity0, entity1, ..., entity63]
 * Total: 65 uint32_t values per cell (260 bytes)
 */
struct SpatialCell {
    uint32_t entityCount;                                    // Number of entities in this cell
    uint32_t entities[SpatialMapConfig::MAX_ENTITIES_PER_CELL]; // Entity indices
    
    SpatialCell() : entityCount(0) {
        std::fill(std::begin(entities), std::end(entities), 0xFFFFFFFF); // Invalid entity marker
    }
};

/**
 * Spatial map buffer for GPU spatial partitioning
 * Single responsibility: manage spatial grid data on GPU
 */
class SpatialMapBuffer : public BufferBase {
public:
    SpatialMapBuffer();
    ~SpatialMapBuffer() override;
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities);
    
    // Hide the base class initialize method to avoid confusion
    using BufferBase::initialize;
    
    // Spatial map specific properties
    uint32_t getGridResolution() const { return SpatialMapConfig::GRID_RESOLUTION; }
    float getCellSize() const { return SpatialMapConfig::CELL_SIZE; }
    float getWorldSize() const { return SpatialMapConfig::WORLD_SIZE; }
    uint32_t getMaxEntitiesPerCell() const { return SpatialMapConfig::MAX_ENTITIES_PER_CELL; }
    
    // Clear all cells (reset entity counts to 0)
    bool clearAllCells();

protected:
    // Override from BufferBase
    const char* getBufferTypeName() const override { return "SpatialMapBuffer"; }
};

/**
 * Entity-to-cell mapping buffer
 * Single responsibility: track which cell each entity belongs to
 */
class EntityCellBuffer : public BufferBase {
public:
    EntityCellBuffer();
    ~EntityCellBuffer() override;
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities);
    
    // Hide the base class initialize method to avoid confusion
    using BufferBase::initialize;

protected:
    // Override from BufferBase
    const char* getBufferTypeName() const override { return "EntityCellBuffer"; }
};

/**
 * Spatial map coordinator
 * Single responsibility: coordinate spatial mapping buffers and operations
 */
class SpatialMapCoordinator {
public:
    SpatialMapCoordinator();
    ~SpatialMapCoordinator();
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities);
    void cleanup();
    
    // Buffer access
    VkBuffer getSpatialMapBuffer() const { return spatialMapBuffer.getBuffer(); }
    VkBuffer getEntityCellBuffer() const { return entityCellBuffer.getBuffer(); }
    
    // Buffer properties
    VkDeviceSize getSpatialMapBufferSize() const { return spatialMapBuffer.getSize(); }
    VkDeviceSize getEntityCellBufferSize() const { return entityCellBuffer.getSize(); }
    
    // Spatial map properties
    uint32_t getGridResolution() const { return spatialMapBuffer.getGridResolution(); }
    float getCellSize() const { return spatialMapBuffer.getCellSize(); }
    float getWorldSize() const { return spatialMapBuffer.getWorldSize(); }
    uint32_t getMaxEntitiesPerCell() const { return spatialMapBuffer.getMaxEntitiesPerCell(); }
    
    // Operations
    bool clearSpatialMap();
    bool uploadEntityCellData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Utility functions for CPU-side calculations
    static uint32_t worldToCell(float worldX, float worldY);
    static glm::ivec2 worldToCellCoords(float worldX, float worldY);
    static glm::vec2 cellToWorld(uint32_t cellIndex);
    static uint32_t cellCoordsToIndex(uint32_t gridX, uint32_t gridY);

private:
    SpatialMapBuffer spatialMapBuffer;
    EntityCellBuffer entityCellBuffer;
    
    uint32_t maxEntities = 0;
    bool initialized = false;
};