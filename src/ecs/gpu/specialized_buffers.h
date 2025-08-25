#pragma once

#include "buffer_base.h"
#include <glm/glm.hpp>

/**
 * Specialized buffer classes following Single Responsibility Principle
 * Each class manages exactly one type of entity data
 */

// SINGLE responsibility: velocity data management
class VelocityBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "Velocity"; }
};

// SINGLE responsibility: movement parameters management
class MovementParamsBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "MovementParams"; }
};

// SINGLE responsibility: movement center positions management (3D support)
class MovementCentersBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "MovementCenters"; }
};

// SINGLE responsibility: runtime state management
class RuntimeStateBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "RuntimeState"; }
};

// SINGLE responsibility: rotation state management
class RotationStateBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "RotationState"; }
};

// SINGLE responsibility: color data management
class ColorBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "Color"; }
};

// SINGLE responsibility: model matrix management
class ModelMatrixBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::mat4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "ModelMatrix"; }
};

// SINGLE responsibility: position data management
class PositionBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxEntities) {
        return BufferBase::initialize(context, resourceCoordinator, maxEntities, sizeof(glm::vec4), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "Position"; }
};

// SINGLE responsibility: spatial cell metadata (bucketed hash table)
class SpatialMapBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t gridSize = 32768) {
        // Spatial map stores pairs: [entityCount, entityOffset] per cell (2 * uint32_t per cell)
        return BufferBase::initialize(context, resourceCoordinator, gridSize, 2 * sizeof(uint32_t), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "SpatialMap"; }
};

// SINGLE responsibility: spatial entity indices (flat array grouped by cell)
class SpatialEntitiesBuffer : public BufferBase {
public:
    using BufferBase::initialize; // Bring base class initialize into scope
    
    bool initialize(const VulkanContext& context, ResourceCoordinator* resourceCoordinator, uint32_t maxSpatialEntries) {
        // Flat array of entity indices, grouped by cell. Size = entities * average_cells_per_entity
        // For 80k entities with ~4 cells per entity average = 320k entries
        return BufferBase::initialize(context, resourceCoordinator, maxSpatialEntries, sizeof(uint32_t), 0);
    }
    
protected:
    const char* getBufferTypeName() const override { return "SpatialEntities"; }
};