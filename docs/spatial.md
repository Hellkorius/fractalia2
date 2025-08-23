# Spatial Map Implementation

## Overview
Lock-free spatial hash grid for entity collision detection and spatial queries. Maps 2D world positions to 64x64 grid cells using atomic linked lists.

## Constants
```glsl
const float CELL_SIZE = 2.0f;           // World units per cell
const uint GRID_WIDTH = 64;             // Grid dimensions (power of 2)
const uint GRID_HEIGHT = 64;
const uint SPATIAL_MAP_SIZE = 4096;     // Total cells (64×64)
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
uint spatialHash(vec2 position) {
    ivec2 gridCoord = ivec2(floor(position / CELL_SIZE));
    uint x = uint(gridCoord.x) & (GRID_WIDTH - 1);   // Wrap with bitwise AND
    uint y = uint(gridCoord.y) & (GRID_HEIGHT - 1);
    return x + y * GRID_WIDTH;
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
- 4096 threads clear 4096 cells in parallel
- Synchronization barrier ensures clearing completes before entity processing

### 2. Physics Integration
- Standard velocity integration and position updates
- No spatial operations during this phase

### 3. Spatial Map Update
```glsl
void updateSpatialMap(uint entityIndex, vec2 position) {
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
const uint32_t spatialClearWorkgroups = (4096 + 63) / 64;  // 64 workgroups
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
bool readbackEntityAtPositionSafe(glm::vec2 worldPos, EntityDebugInfo& info) {
    vkDeviceWaitIdle(device);  // Wait for GPU compute completion
    return readbackEntityAtPosition(worldPos, info);
}
```

**Search Process**:
1. Calculate clicked cell using same hash function as GPU
2. Search 5×5 grid around clicked cell (25 cells total)
3. Read linked list head from each cell
4. Find closest entity by distance to click point
5. Map GPU index back to ECS entity ID

## Performance Characteristics
- **Clearing**: O(1) parallel clear of all cells
- **Insertion**: O(1) atomic insertion per entity  
- **Query**: O(k) where k = entities per cell
- **Memory**: 32KB for 4096 cells (8 bytes per uvec2)
- **Collision**: Lock-free, no GPU stalls or deadlocks