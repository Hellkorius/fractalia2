# Spatial Map Implementation

## Overview
Lock-free 3D spatial hash grid for entity collision detection and spatial queries. Maps 3D world positions to 32x32x32 grid cells using atomic linked lists.

## Constants
```glsl
const float CELL_SIZE = 2.0f;           // World units per cell (larger for 3D cubes)
const uint GRID_WIDTH = 32;             // 3D grid dimensions (power of 2)
const uint GRID_HEIGHT = 32;
const uint GRID_DEPTH = 32;
const uint SPATIAL_MAP_SIZE = 32768;    // Total cells (32³ = 32,768)
const uint NULL_INDEX = 0xFFFFFFFF;     // Null pointer sentinel
```

## Data Structure
```glsl
layout(std430, binding = 7) buffer SpatialMapBuffer {
    uvec2 spatialCells[];  // [entityId, nextIndex] per cell
} spatialMap;
```

Each cell contains:
- `.x`: Most recent entity ID in this cell  
- `.y`: Previous entity ID (linked list chain)

## Hash Function
```glsl
uint spatialHash(vec3 position) {
    ivec3 gridCoord = ivec3(floor(position / CELL_SIZE));
    uint x = uint(gridCoord.x) & (GRID_WIDTH - 1);    // Wrap with bitwise AND
    uint y = uint(gridCoord.y) & (GRID_HEIGHT - 1);
    uint z = uint(gridCoord.z) & (GRID_DEPTH - 1);
    return x + y * GRID_WIDTH + z * GRID_WIDTH * GRID_HEIGHT;
}
```

## Frame Process (physics.comp)

### 1. Clear Phase
```glsl
// Each thread clears one cell
if (gl_GlobalInvocationID.x < SPATIAL_MAP_SIZE) {
    spatialMap.spatialCells[gl_GlobalInvocationID.x] = uvec2(NULL_INDEX, NULL_INDEX);
}
barrier();
memoryBarrierShared();
```
- 32,768 threads clear 32,768 cells in parallel (3D grid)
- Synchronization barrier ensures clearing completes before entity processing

### 2. Physics Integration
- Standard velocity integration and position updates
- No spatial operations during this phase

### 3. Spatial Map Update
```glsl
void updateSpatialMap(uint entityIndex, vec3 position) {
    uint cellIndex = spatialHash(position);
    
    // Atomic compare-swap loop for race-free linked list insertion
    uint expectedHead;
    do {
        expectedHead = spatialMap.spatialCells[cellIndex].x;
    } while (atomicCompSwap(spatialMap.spatialCells[cellIndex].x, expectedHead, entityIndex) != expectedHead);
    
    // Store previous head as next pointer
    spatialMap.spatialCells[cellIndex].y = expectedHead;
}
```

## Atomic Safety
**Problem**: Multiple threads updating same cell simultaneously
**Solution**: Compare-swap loop ensures atomic head updates:
1. Read current head atomically
2. Attempt to replace head with our entity
3. Retry if another thread modified head between read and write
4. Store old head as our next pointer (building linked list)

## Dispatch Configuration
```cpp
// Calculate workgroups for both spatial clearing and entity processing
const uint32_t spatialClearWorkgroups = (32768 + 63) / 64;  // 512 workgroups (3D grid)
const uint32_t entityWorkgroups = (entityCount + 63) / 64;
const uint32_t totalWorkgroups = std::max(spatialClearWorkgroups, entityWorkgroups);
```

## CPU-GPU Mapping
**GPU Index**: Sequential array indices (0, 1, 2, ..., N)
**ECS Entity**: Unique Flecs IDs (0x3ea, 0x7b2, ...)  
**Mapping**: `gpuIndexToECSEntity[gpuIndex] → flecs::entity`

## Debug Readback
```cpp
// Synchronized readback prevents race conditions
bool readbackEntityAtPositionSafe(glm::vec3 worldPos, EntityDebugInfo& info) {
    vkDeviceWaitIdle(device);  // Wait for GPU compute completion
    return readbackEntityAtPosition(worldPos, info);
}
```

**Search Process**:
1. Calculate clicked cell using same 3D hash function as GPU
2. Search 3×3×3 grid around clicked cell (27 cells total in 3D space)
3. Read linked list head from each cell
4. Find closest entity by 3D distance to click point
5. Map GPU index back to ECS entity ID

## Performance Characteristics
- **Clearing**: O(1) parallel clear of all cells (3D optimized)
- **Insertion**: O(1) atomic insertion per entity  
- **Query**: O(k) where k = entities per cell
- **Memory**: 256KB for 32,768 cells (8 bytes per uvec2, 8x larger for 3D)
- **Collision**: Lock-free, no GPU stalls or deadlocks
- **3D Collision**: Layered approach (spatial → sphere → cube) for optimal performance