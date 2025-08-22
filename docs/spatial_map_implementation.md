# Spatial Map System Implementation Guide

## Overview
This document details the complete step-by-step process for adding a GPU-based spatial map system to the Fractalia2 engine. The spatial map divides the world into a 128x128 grid and tracks which entities are in each cell for fast spatial queries.

## Complete Process: Adding Spatial Map System to GPU Pipeline

### 1. **Design Data Structures & Configuration**

**File: `src/ecs/gpu/spatial_map_buffers.h`**
```cpp
// Configuration constants
struct SpatialMapConfig {
    static constexpr float WORLD_SIZE = 2000.0f;           // -1000 to +1000
    static constexpr uint32_t GRID_RESOLUTION = 128;       // 128x128 grid
    static constexpr float CELL_SIZE = WORLD_SIZE / GRID_RESOLUTION; // ~15.625 units
    static constexpr uint32_t MAX_ENTITIES_PER_CELL = 64;
    static constexpr uint32_t TOTAL_CELLS = GRID_RESOLUTION * GRID_RESOLUTION;
    static constexpr uint32_t CELL_DATA_SIZE = MAX_ENTITIES_PER_CELL + 1; // +1 for count
};

// GPU cell structure: [count, entity0, entity1, ..., entity63]
struct SpatialCell {
    uint32_t entityCount;
    uint32_t entities[SpatialMapConfig::MAX_ENTITIES_PER_CELL];
};
```

### 2. **Create Specialized Buffer Classes (SRP Pattern)**

**File: `src/ecs/gpu/spatial_map_buffers.h`**
```cpp
// Spatial map buffer (manages grid cells)
class SpatialMapBuffer : public BufferBase {
    const char* getBufferTypeName() const override { return "SpatialMapBuffer"; }
    bool clearAllCells(); // Reset all cell counts to 0
};

// Entity-to-cell mapping buffer (tracks which cell each entity belongs to)
class EntityCellBuffer : public BufferBase {
    const char* getBufferTypeName() const override { return "EntityCellBuffer"; }
};

// Coordinator class (orchestrates both buffers)
class SpatialMapCoordinator {
    SpatialMapBuffer spatialMapBuffer;
    EntityCellBuffer entityCellBuffer;
    // CPU utility functions for world↔cell coordinate conversion
};
```

**Implementation: `src/ecs/gpu/spatial_map_buffers.cpp`**
- Use `BufferBase::initialize()` with proper usage flags
- Spatial map: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT`
- Entity cell: Same usage flags, one uint32_t per entity

### 3. **Implement Compute Shader**

**File: `src/shaders/spatial_map_update.comp`**
```glsl
#version 450
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Push constants for configuration
layout(push_constant) uniform SpatialPushConstants {
    uint entityCount;
    uint gridResolution;    // 128
    float cellSize;         // ~15.625
    float worldSize;        // 2000.0
    uint maxEntitiesPerCell; // 64
    uint clearMapFirst;     // 1 to clear, 0 to skip
} pc;

// Input/Output buffers
layout(std430, binding = 0) buffer readonly PositionBuffer { vec4 positions[]; };
layout(std430, binding = 1) buffer writeonly EntityCellBuffer { uint entityCells[]; };
layout(std430, binding = 2) buffer coherent SpatialMapBuffer { uint spatialMap[]; };

void main() {
    // Clear spatial map if first thread and requested
    if (pc.clearMapFirst != 0 && gl_GlobalInvocationID.x == 0) {
        // Clear all cells in parallel
    }
    
    // Calculate entity's cell index from position
    uint cellIndex = worldToCell(positions[entityIndex].xy);
    
    // Store mapping
    entityCells[entityIndex] = cellIndex;
    
    // Atomically add entity to spatial map cell
    uint slotIndex = atomicAdd(spatialMap[cellOffset], 1);
    if (slotIndex < pc.maxEntitiesPerCell) {
        spatialMap[cellOffset + 1 + slotIndex] = entityIndex;
    }
}
```

### 4. **Create Frame Graph Compute Node**

**File: `src/vulkan/nodes/spatial_map_compute_node.h`**
```cpp
class SpatialMapComputeNode : public FrameGraphNode {
    // Resource dependencies
    FrameGraphTypes::ResourceId positionBufferId;        // Input
    FrameGraphTypes::ResourceId spatialMapBufferId;     // Output
    FrameGraphTypes::ResourceId entityCellBufferId;     // Output
    
    // Push constants matching shader
    struct SpatialPushConstants {
        uint32_t entityCount, gridResolution;
        float cellSize, worldSize;
        uint32_t maxEntitiesPerCell, clearMapFirst;
    };
};
```

**Implementation: `src/vulkan/nodes/spatial_map_compute_node.cpp`**
- Declare inputs: `{positionBufferId, ResourceAccess::Read, PipelineStage::ComputeShader}`
- Declare outputs: `{spatialMapBufferId, ResourceAccess::Write}`, `{entityCellBufferId, ResourceAccess::Write}`
- Use chunked dispatch for 80K+ entities
- Fix method calls: `frameGraph.getContext()` not `getVulkanContext()`
- Fix dispatch: `dispatch.groupCountX/Y/Z` not `dispatch.workgroups`

### 5. **Add Descriptor Layout Support**

**File: `src/vulkan/pipelines/descriptor_layout_manager.h`**
```cpp
namespace DescriptorLayoutPresets {
    DescriptorLayoutSpec createSpatialMapComputeLayout();
}
```

**Implementation: `src/vulkan/pipelines/descriptor_layout_manager.cpp`**
```cpp
DescriptorLayoutSpec createSpatialMapComputeLayout() {
    DescriptorLayoutSpec spec;
    spec.layoutName = "SpatialMapComputeLayout";
    
    // Binding 0: Position Buffer (read-only)
    DescriptorBinding positionBinding{};
    positionBinding.binding = 0;
    positionBinding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    positionBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: EntityCell Buffer (write-only)
    // Binding 2: Spatial Map Buffer (read-write)
    
    spec.bindings = {positionBinding, entityCellBinding, spatialMapBinding};
    return spec;
}
```

### 6. **Add Pipeline Preset**

**File: `src/vulkan/pipelines/compute_pipeline_manager.h`**
```cpp
namespace ComputePipelinePresets {
    ComputePipelineState createSpatialMapUpdateState(VkDescriptorSetLayout descriptorLayout);
}
```

**Implementation: `src/vulkan/pipelines/compute_pipeline_manager.cpp`**
```cpp
ComputePipelineState createSpatialMapUpdateState(VkDescriptorSetLayout descriptorLayout) {
    ComputePipelineState state{};
    state.shaderPath = "shaders/spatial_map_update.comp.spv";
    state.descriptorSetLayouts.push_back(descriptorLayout);
    state.workgroupSizeX = THREADS_PER_WORKGROUP; // 64
    
    // Push constants for spatial map parameters
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstant.size = sizeof(uint32_t) * 6 + sizeof(float) * 2;
    state.pushConstantRanges.push_back(pushConstant);
    
    return state;
}
```

### 7. **Integrate with Buffer Management**

**File: `src/ecs/gpu/entity_buffer_manager.h`**
```cpp
#include "spatial_map_buffers.h"

class EntityBufferManager {
    // Add spatial map coordinator
    SpatialMapCoordinator spatialMapCoordinator;
    
    // Add access methods
    VkBuffer getSpatialMapBuffer() const;
    VkBuffer getEntityCellBuffer() const;
    bool clearSpatialMap();
};
```

**Implementation: `src/ecs/gpu/entity_buffer_manager.cpp`**
```cpp
bool EntityBufferManager::initialize(...) {
    // Initialize spatial map coordinator
    if (!spatialMapCoordinator.initialize(context, resourceCoordinator, maxEntities)) {
        return false;
    }
}

void EntityBufferManager::cleanup() {
    spatialMapCoordinator.cleanup(); // Add to cleanup order
}
```

### 8. **Extend GPUEntityManager Interface**

**File: `src/ecs/gpu/gpu_entity_manager.h`**
```cpp
class GPUEntityManager {
    // Add spatial map buffer access
    VkBuffer getSpatialMapBuffer() const { return bufferManager.getSpatialMapBuffer(); }
    VkBuffer getEntityCellBuffer() const { return bufferManager.getEntityCellBuffer(); }
    
    // Add spatial map operations
    bool clearSpatialMap() { return bufferManager.clearSpatialMap(); }
    uint32_t getGridResolution() const { return bufferManager.getGridResolution(); }
    float getCellSize() const { return bufferManager.getCellSize(); }
};
```

### 9. **Update Descriptor Set Management**

**File: `src/ecs/gpu/entity_descriptor_manager.h`**
```cpp
class EntityDescriptorManager {
    // Add spatial map descriptor set support
    VkDescriptorSetLayout spatialMapDescriptorSetLayout = VK_NULL_HANDLE;
    vulkan_raii::DescriptorPool spatialMapDescriptorPool;
    VkDescriptorSet spatialMapDescriptorSet = VK_NULL_HANDLE;
    
    // Add access methods
    VkDescriptorSet getSpatialMapDescriptorSet() const;
    bool createSpatialMapDescriptorSets(VkDescriptorSetLayout layout);
};
```

### 10. **Update Build System**

**File: `compile-shaders.sh`**
```bash
# Compile compute shader (spatial map update)
glslangValidator -V src/shaders/spatial_map_update.comp -o src/shaders/compiled/spatial_map_update.comp.spv
cp src/shaders/compiled/spatial_map_update.comp.spv build/shaders/
```

### 11. **Fix Compilation Issues**

**Method Hiding Warnings:**
```cpp
// In spatial map buffer classes
using BufferBase::initialize; // Hide base class method signature conflicts
```

**Shader Compilation Errors:**
```glsl
// Fix: Don't use push constants in global const initialization
// Wrong: const float HALF_WORLD = pc.worldSize * 0.5;
// Right: float halfWorld = pc.worldSize * 0.5; // Inside function
```

**FrameGraph API Usage:**
```cpp
// Fix: Use correct method names
frameGraph.getContext() // not getVulkanContext()
dispatch.groupCountX/Y/Z // not dispatch.workgroups.x/y/z
```

## **Integration Points:**

1. **Pipeline Flow**: Movement Compute → Spatial Map Compute → Graphics Render
2. **Resource Dependencies**: Position buffer output → Spatial map input
3. **Descriptor Sets**: 3-binding layout (position, entity-cell, spatial-map)
4. **Memory Layout**: 128x128 grid, 64 entities/cell, atomic operations
5. **Performance**: Chunked dispatch, GPU timeout detection, adaptive workloads

## **Architecture Overview**

### Data Flow
```
ECS Components → GPUEntityManager (SoA conversion) → GPU Buffers
                        ↓
Movement Compute → Position Updates → Spatial Map Compute → Grid Updates
     ↓                    ↓                     ↓              ↓
Velocity Calc     Position Buffer     Entity→Cell Map    Spatial Grid
```

### Memory Layout
- **Grid**: 128x128 cells covering 2000x2000 world (-1000 to +1000)
- **Cell Size**: ~15.625 units per cell for optimal spatial resolution
- **Cell Data**: [count, entity0, entity1, ..., entity63] = 260 bytes per cell
- **Total Memory**: ~4.2 MB for spatial map + entity-cell mapping

### GPU Performance Features
- **Atomic Operations**: Thread-safe cell updates without locks
- **Chunked Dispatch**: Prevents GPU timeouts with 80K+ entities
- **Adaptive Workloads**: Dynamic workgroup limits based on GPU health
- **Automatic Clearing**: Map reset each frame with minimal overhead

## **Usage Benefits**

1. **Spatial Queries**: O(1) cell lookup instead of O(n) entity iteration
2. **Collision Detection**: Only check entities in same/neighboring cells
3. **Culling**: Only process entities in visible cells
4. **LOD Systems**: Different detail levels based on spatial density
5. **Neighbor Finding**: Up to 64 entity indices per cell for fast neighbor queries

This implementation creates a complete spatial partitioning system that efficiently tracks 80,000+ entities across a spatial grid, updating every frame with minimal GPU overhead through atomic operations and chunked dispatch.