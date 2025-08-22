#pragma once

#include "specialized_buffers.h"
#include "position_buffer_coordinator.h"
#include "spatial_map_buffers.h"
#include "buffer_upload_service.h"
#include <vulkan/vulkan.h>
#include <memory>

// Forward declarations
class VulkanContext;
class ResourceCoordinator;

/**
 * REFACTORED: Entity buffer manager using SRP-compliant specialized buffer classes
 * Single responsibility: coordinate specialized buffer components for entity rendering
 */
class EntityBufferManager {
public:
    EntityBufferManager();
    ~EntityBufferManager();

    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities);
    void cleanup();
    
    // SoA buffer access - delegated to specialized buffers
    VkBuffer getVelocityBuffer() const { return velocityBuffer.getBuffer(); }
    VkBuffer getMovementParamsBuffer() const { return movementParamsBuffer.getBuffer(); }
    VkBuffer getRuntimeStateBuffer() const { return runtimeStateBuffer.getBuffer(); }
    VkBuffer getColorBuffer() const { return colorBuffer.getBuffer(); }
    VkBuffer getModelMatrixBuffer() const { return modelMatrixBuffer.getBuffer(); }
    
    // Position buffers - delegated to coordinator
    VkBuffer getPositionBuffer() const { return positionCoordinator.getPrimaryBuffer(); }
    VkBuffer getPositionBufferAlternate() const { return positionCoordinator.getAlternateBuffer(); }
    VkBuffer getCurrentPositionBuffer() const { return positionCoordinator.getCurrentBuffer(); }
    VkBuffer getTargetPositionBuffer() const { return positionCoordinator.getTargetBuffer(); }
    
    // Spatial map buffers - delegated to coordinator
    VkBuffer getSpatialMapBuffer() const { return spatialMapCoordinator.getSpatialMapBuffer(); }
    VkBuffer getEntityCellBuffer() const { return spatialMapCoordinator.getEntityCellBuffer(); }
    
    
    // Ping-pong buffer access - delegated to coordinator
    VkBuffer getComputeWriteBuffer(uint32_t frameIndex) const { return positionCoordinator.getComputeWriteBuffer(frameIndex); }
    VkBuffer getGraphicsReadBuffer(uint32_t frameIndex) const { return positionCoordinator.getGraphicsReadBuffer(frameIndex); }
    
    // Buffer properties - delegated to specialized buffers
    VkDeviceSize getVelocityBufferSize() const { return velocityBuffer.getSize(); }
    VkDeviceSize getMovementParamsBufferSize() const { return movementParamsBuffer.getSize(); }
    VkDeviceSize getRuntimeStateBufferSize() const { return runtimeStateBuffer.getSize(); }
    VkDeviceSize getColorBufferSize() const { return colorBuffer.getSize(); }
    VkDeviceSize getModelMatrixBufferSize() const { return modelMatrixBuffer.getSize(); }
    VkDeviceSize getPositionBufferSize() const { return positionCoordinator.getBufferSize(); }
    VkDeviceSize getSpatialMapBufferSize() const { return spatialMapCoordinator.getSpatialMapBufferSize(); }
    VkDeviceSize getEntityCellBufferSize() const { return spatialMapCoordinator.getEntityCellBufferSize(); }
    uint32_t getMaxEntities() const { return maxEntities; }
    
    
    // Data upload - using shared upload service
    
    // Typed upload methods for better API
    bool uploadVelocityData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadMovementParamsData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadRuntimeStateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadColorData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadModelMatrixData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool uploadPositionDataToAllBuffers(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Spatial map operations
    bool clearSpatialMap();
    bool uploadEntityCellData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Spatial map properties
    uint32_t getGridResolution() const { return spatialMapCoordinator.getGridResolution(); }
    float getCellSize() const { return spatialMapCoordinator.getCellSize(); }
    float getWorldSize() const { return spatialMapCoordinator.getWorldSize(); }
    uint32_t getMaxEntitiesPerCell() const { return spatialMapCoordinator.getMaxEntitiesPerCell(); }

private:
    // Configuration
    uint32_t maxEntities = 0;
    
    // Specialized buffer components (SRP-compliant)
    VelocityBuffer velocityBuffer;
    MovementParamsBuffer movementParamsBuffer;
    RuntimeStateBuffer runtimeStateBuffer;
    ColorBuffer colorBuffer;
    ModelMatrixBuffer modelMatrixBuffer;
    
    // Position buffer coordination
    PositionBufferCoordinator positionCoordinator;
    
    // Spatial map coordination
    SpatialMapCoordinator spatialMapCoordinator;
    
    // Shared upload service
    BufferUploadService uploadService;
    
};

