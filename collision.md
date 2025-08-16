# GPU-Driven Collision Detection Implementation Plan

## Overview
This document outlines the implementation plan for adding GPU-driven collision detection with spatial hashing to the Fractalia2 engine. The system will detect collisions between entities entirely on the GPU and dispatch events through a global event manager.

## Architecture Summary
- **Spatial Hash Grid**: 512×512 uniform grid for spatial partitioning
- **Two-Pass Compute**: Hash population followed by collision pair generation
- **Event Dispatch**: Global CollisionEventManager for frame-based event processing
- **No CPU Fallback**: Pure GPU-driven collision detection only

---

## Phase 1: Core Infrastructure & Memory Layout

### 1.1 Extend GPUEntity Structure
**File**: `src/ecs/gpu_entity_manager.h`

**Objective**: Add collision data while maintaining 128-byte alignment and cache optimization.

**Changes**:
```cpp
struct GPUEntity {
    // Existing hot data (positions 0-3): 64 bytes
    glm::vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // center.xyz, movementType
    glm::vec4 runtimeState;       // totalTime, initialized, stateTimer, entityState
    glm::vec4 color;              // RGBA color
    
    // Extended data (positions 4-7): 64 bytes
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
    
    // Replace unused matrix components with collision data:
    // modelMatrix[3] becomes: vec4(radius, layer, mask, reserved)
};

// Update fromECS() method to populate collision data from new Collider component
static GPUEntity fromECS(const Transform&, const Renderable&, const MovementPattern&, const Collider&);
```

**Static Assertion**: Verify 128-byte size maintained after changes.

### 1.2 Add Collider ECS Component
**File**: `src/ecs/component.h`

**Objective**: Define collision properties for entities.

**Changes**:
```cpp
struct Collider {
    float radius = 0.5f;          // Collision radius
    uint32_t layer = 0;           // Collision layer (0-31)
    uint32_t mask = ~0u;          // Layer mask - what this entity collides with
    bool enabled = true;          // Runtime collision toggle
    
    // Helper methods
    bool canCollideWith(const Collider& other) const {
        return enabled && other.enabled && 
               (mask & (1u << other.layer)) && 
               (other.mask & (1u << layer));
    }
};
```

### 1.3 Collision Event System
**Files**: 
- `src/ecs/collision_event_manager.h` (new)
- `src/ecs/collision_event_manager.cpp` (new)

**Objective**: Global event manager for collision dispatch.

**Core Structure**:
```cpp
struct CollisionEvent {
    uint32_t entityA;           // First entity ID
    uint32_t entityB;           // Second entity ID
    glm::vec3 contactPoint;     // Collision point in world space
    glm::vec3 normal;           // Collision normal (A→B)
    float penetrationDepth;     // How deep the collision is
    uint32_t frameNumber;       // Frame when collision occurred
};

class CollisionEventManager {
public:
    // Frame management
    void beginFrame(uint32_t frameNumber);
    void endFrame();
    void transferFromGPU();
    
    // Event access
    const std::vector<CollisionEvent>& getFrameEvents() const;
    void clearFrameEvents();
    
    // System integration
    void processEvents(flecs::world& world);
    
    // Statistics
    uint32_t getCollisionCount() const;
    uint32_t getMaxCollisionsPerFrame() const;

private:
    std::vector<CollisionEvent> frameEvents;
    std::vector<CollisionEvent> stagingEvents;
    uint32_t currentFrame = 0;
    uint32_t maxCollisionsPerFrame = 0;
    
    // GPU transfer state
    bool transferInProgress = false;
    std::unique_ptr<GPUBufferRing> collisionBuffer;
};
```

---

## Phase 2: GPU Memory Buffers & Resource Management

### 2.1 Spatial Hash Buffers
**File**: `src/ecs/gpu_entity_manager.h`

**Objective**: Add GPU buffers for spatial hashing infrastructure.

**New Buffer Members**:
```cpp
class GPUEntityManager {
private:
    // Existing buffers...
    
    // Spatial hash infrastructure
    std::unique_ptr<GPUBufferRing> spatialHashBuffer;     // Cell offset + count pairs
    std::unique_ptr<GPUBufferRing> entityIndexBuffer;     // Sorted entity indices
    std::unique_ptr<GPUBufferRing> collisionPairsBuffer;  // Output collision pairs
    
    // Hash grid constants
    static constexpr uint32_t GRID_WIDTH = 512;
    static constexpr uint32_t GRID_HEIGHT = 512;
    static constexpr uint32_t GRID_SIZE = GRID_WIDTH * GRID_HEIGHT;
    static constexpr uint32_t MAX_COLLISION_PAIRS = 65536; // ~2MB buffer
    
    // Buffer creation methods
    bool createSpatialHashBuffers();
    bool createCollisionBuffers();
};
```

**Buffer Sizes**:
- Spatial Hash: `GRID_SIZE * 8 bytes` = 2MB (offset + count per cell)
- Entity Index: `MAX_ENTITIES * 4 bytes` = 512KB (entity index per entity)
- Collision Pairs: `MAX_COLLISION_PAIRS * 32 bytes` = 2MB (collision event data)

### 2.2 Descriptor Set Extension
**File**: `src/vulkan/vulkan_pipeline.h`

**Objective**: Extend compute descriptor set layout for collision buffers.

**Changes**:
```cpp
class VulkanPipeline {
private:
    // Collision compute pipeline
    VkPipelineLayout collisionPipelineLayout = VK_NULL_HANDLE;
    VkPipeline collisionPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout collisionDescriptorSetLayout = VK_NULL_HANDLE; // 7 bindings total
    
    bool createCollisionDescriptorSetLayout();
    bool createCollisionPipeline();
};
```

**Descriptor Layout** (7 bindings):
- Binding 0: GPUEntity buffer (read-only)
- Binding 1: Current positions (read-only)
- Binding 2: Spatial hash grid (read/write)
- Binding 3: Entity index buffer (write-only)
- Binding 4: Cell counter buffer (read/write) 
- Binding 5: Collision pairs output (write-only)
- Binding 6: Collision statistics (write-only)

---

## Phase 3: Collision Detection Compute Shaders

### 3.1 Spatial Hash Population Shader
**File**: `src/shaders/collision_hash.comp` (new)

**Objective**: Populate spatial hash grid with entity positions.

**Algorithm**:
```glsl
#version 450
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

// Push constants
layout(push_constant) uniform CollisionPushConstants {
    uint entityCount;
    float gridCellSize;
    vec2 gridOrigin;
    uint frameNumber;
} pc;

// Input buffers
layout(std430, binding = 0) readonly buffer GPUEntityBuffer { /* entity data */ };
layout(std430, binding = 1) readonly buffer PositionBuffer { vec4 positions[]; };

// Output buffers  
layout(std430, binding = 2) buffer SpatialHashBuffer { uvec2 hashGrid[]; }; // offset + count
layout(std430, binding = 3) buffer EntityIndexBuffer { uint entityIndices[]; };
layout(std430, binding = 4) buffer CellCounterBuffer { uint cellCounters[]; };

void main() {
    uint entityIndex = gl_GlobalInvocationID.x;
    if (entityIndex >= pc.entityCount) return;
    
    // Extract entity position and collision data
    vec3 position = positions[entityIndex].xyz;
    // Extract radius from GPUEntity.modelMatrix[3].x
    float radius = entities.entityData[entityIndex * 8 + 7].x; // modelMatrix[3].x
    
    // Skip if collision disabled (radius <= 0)
    if (radius <= 0.0) return;
    
    // Calculate grid cell
    ivec2 gridPos = ivec2(floor((position.xy - pc.gridOrigin) / pc.gridCellSize));
    if (gridPos.x < 0 || gridPos.x >= GRID_WIDTH || 
        gridPos.y < 0 || gridPos.y >= GRID_HEIGHT) return;
    
    uint cellIndex = gridPos.y * GRID_WIDTH + gridPos.x;
    
    // Atomically increment cell counter and get insertion index
    uint insertIndex = atomicAdd(cellCounters[cellIndex], 1);
    
    // Store entity index in sorted buffer
    // Note: Requires prefix sum pass to convert cellCounters to offsets
    uint globalIndex = hashGrid[cellIndex].x + insertIndex; // offset + local index
    entityIndices[globalIndex] = entityIndex;
}
```

### 3.2 Collision Pair Generation Shader  
**File**: `src/shaders/collision_pairs.comp` (new)

**Objective**: Generate collision pairs from populated spatial hash.

**Algorithm**:
```glsl
#version 450
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

// Same push constants and input buffers as hash shader...

layout(std430, binding = 5) buffer CollisionPairsBuffer {
    // CollisionEvent structure (32 bytes each)
    uint collisionPairs[]; // Packed: entityA, entityB, contactPoint.xyz, normal.xyz, depth, frame
};

layout(std430, binding = 6) buffer CollisionStatsBuffer {
    uint collisionCount;
    uint totalChecks;
    uint gridOccupancy;
    uint frameNumber;
} stats;

void main() {
    uint entityIndex = gl_GlobalInvocationID.x;
    if (entityIndex >= pc.entityCount) return;
    
    vec3 positionA = positions[entityIndex].xyz;
    float radiusA = entities.entityData[entityIndex * 8 + 7].x;
    uint layerA = floatBitsToUint(entities.entityData[entityIndex * 8 + 7].y);
    uint maskA = floatBitsToUint(entities.entityData[entityIndex * 8 + 7].z);
    
    if (radiusA <= 0.0) return;
    
    // Calculate grid cell for entity A
    ivec2 gridPos = ivec2(floor((positionA.xy - pc.gridOrigin) / pc.gridCellSize));
    
    // Check neighboring cells (3x3 grid around entity)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            ivec2 neighborPos = gridPos + ivec2(dx, dy);
            if (neighborPos.x < 0 || neighborPos.x >= GRID_WIDTH ||
                neighborPos.y < 0 || neighborPos.y >= GRID_HEIGHT) continue;
                
            uint cellIndex = neighborPos.y * GRID_WIDTH + neighborPos.x;
            uint cellOffset = hashGrid[cellIndex].x;
            uint cellCount = hashGrid[cellIndex].y;
            
            // Check all entities in this cell
            for (uint i = 0; i < cellCount; i++) {
                uint entityB = entityIndices[cellOffset + i];
                if (entityB <= entityIndex) continue; // Avoid duplicate pairs
                
                vec3 positionB = positions[entityB].xyz;
                float radiusB = entities.entityData[entityB * 8 + 7].x;
                uint layerB = floatBitsToUint(entities.entityData[entityB * 8 + 7].y);
                uint maskB = floatBitsToUint(entities.entityData[entityB * 8 + 7].z);
                
                // Layer mask filtering
                if ((maskA & (1u << layerB)) == 0 || (maskB & (1u << layerA)) == 0) continue;
                
                // Distance check
                vec3 delta = positionB - positionA;
                float distance = length(delta);
                float combinedRadius = radiusA + radiusB;
                
                if (distance < combinedRadius && distance > 0.0) {
                    // Collision detected - store collision event
                    uint collisionIndex = atomicAdd(stats.collisionCount, 1);
                    if (collisionIndex < MAX_COLLISION_PAIRS) {
                        // Pack collision data (8 uints = 32 bytes)
                        uint baseIndex = collisionIndex * 8;
                        collisionPairs[baseIndex + 0] = entityIndex;
                        collisionPairs[baseIndex + 1] = entityB;
                        
                        vec3 normal = normalize(delta);
                        vec3 contactPoint = positionA + normal * radiusA;
                        float penetration = combinedRadius - distance;
                        
                        // Pack vec3 as 3 uints (float to uint bit conversion)
                        collisionPairs[baseIndex + 2] = floatBitsToUint(contactPoint.x);
                        collisionPairs[baseIndex + 3] = floatBitsToUint(contactPoint.y);
                        collisionPairs[baseIndex + 4] = floatBitsToUint(contactPoint.z);
                        collisionPairs[baseIndex + 5] = floatBitsToUint(normal.x);
                        collisionPairs[baseIndex + 6] = floatBitsToUint(normal.y);
                        collisionPairs[baseIndex + 7] = floatBitsToUint(normal.z);
                        // penetration and frame stored separately in next iteration
                    }
                }
            }
        }
    }
}
```

---

## Phase 4: Pipeline Integration & Compute Orchestration

### 4.1 Collision Compute Pipeline Class
**Files**:
- `src/vulkan/collision_compute_pipeline.h` (new)
- `src/vulkan/collision_compute_pipeline.cpp` (new)

**Objective**: Manage collision detection compute pipeline execution.

**Core Interface**:
```cpp
class CollisionComputePipeline {
public:
    bool initialize(const VulkanContext& context, VulkanSync* sync, 
                   ResourceContext* resourceContext);
    void cleanup();
    
    // Pipeline execution
    void detectCollisions(VkCommandBuffer commandBuffer, uint32_t entityCount,
                         float gridCellSize, glm::vec2 gridOrigin, uint32_t frameNumber);
    
    // Resource management
    VkDescriptorSet getDescriptorSet() const;
    bool recreateResources();
    
    // Statistics
    uint32_t getLastCollisionCount() const;
    void resetCounters();

private:
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // Pipeline resources
    VkPipeline hashPipeline = VK_NULL_HANDLE;
    VkPipeline pairsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    
    // Execution methods
    void dispatchHashPopulation(VkCommandBuffer cmd, uint32_t entityCount);
    void dispatchPairGeneration(VkCommandBuffer cmd, uint32_t entityCount);
    void insertMemoryBarriers(VkCommandBuffer cmd);
};
```

### 4.2 Renderer Integration
**File**: `src/vulkan_renderer.cpp`

**Objective**: Integrate collision detection into the main render loop.

**Modified Frame Loop**:
```cpp
// In drawFrame() method, after compute command buffer begin:

// 1. COLLISION DETECTION PASS (NEW)
if (gpuEntityManager && gpuEntityManager->getEntityCount() > 0) {
    // Clear collision counters
    collisionPipeline->resetCounters();
    
    // Dispatch collision detection
    float gridCellSize = calculateOptimalCellSize(); // 2.0 * averageEntityRadius
    glm::vec2 gridOrigin = calculateGridOrigin();    // Based on camera bounds
    
    collisionPipeline->detectCollisions(
        computeCommandBuffers[currentFrame],
        gpuEntityManager->getEntityCount(),
        gridCellSize, gridOrigin, frameCounter
    );
    
    // Memory barrier: collision write → movement read
    VkMemoryBarrier collisionBarrier{};
    collisionBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    collisionBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    collisionBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    context->getLoader().vkCmdPipelineBarrier(
        computeCommandBuffers[currentFrame],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &collisionBarrier, 0, nullptr, 0, nullptr
    );
}

// 2. MOVEMENT CALCULATION PASS (EXISTING)
// ... existing movement pipeline dispatch code ...

// 3. MOVEMENT → GRAPHICS BARRIER (EXISTING)
// ... existing transitionBufferLayout() call ...
```

---

## Phase 5: Event Processing & ECS Integration

### 5.1 GPU-to-CPU Data Transfer
**File**: `src/ecs/collision_event_manager.cpp`

**Objective**: Efficiently transfer collision data from GPU to CPU.

**Transfer Implementation**:
```cpp
void CollisionEventManager::transferFromGPU() {
    if (transferInProgress) return;
    
    // Get collision count from GPU statistics buffer
    uint32_t* statsData = static_cast<uint32_t*>(
        collisionStatsBuffer->getStagingData()
    );
    uint32_t collisionCount = statsData[0]; // First uint in stats buffer
    
    if (collisionCount == 0) {
        frameEvents.clear();
        return;
    }
    
    // Clamp to maximum supported collisions
    collisionCount = std::min(collisionCount, MAX_COLLISION_PAIRS);
    
    // Resize frame events vector
    frameEvents.resize(collisionCount);
    
    // Copy collision data from GPU staging buffer
    const uint32_t* collisionData = static_cast<const uint32_t*>(
        collisionPairsBuffer->getStagingData()
    );
    
    // Unpack GPU collision data into CPU collision events
    for (uint32_t i = 0; i < collisionCount; i++) {
        const uint32_t* eventData = &collisionData[i * 8]; // 8 uints per event
        CollisionEvent& event = frameEvents[i];
        
        event.entityA = eventData[0];
        event.entityB = eventData[1];
        
        // Unpack contact point (bits to float conversion)
        event.contactPoint.x = uintBitsToFloat(eventData[2]);
        event.contactPoint.y = uintBitsToFloat(eventData[3]); 
        event.contactPoint.z = uintBitsToFloat(eventData[4]);
        
        // Unpack normal vector
        event.normal.x = uintBitsToFloat(eventData[5]);
        event.normal.y = uintBitsToFloat(eventData[6]);
        event.normal.z = uintBitsToFloat(eventData[7]);
        
        // Additional data from next section of buffer...
        event.penetrationDepth = uintBitsToFloat(eventData[8]);
        event.frameNumber = eventData[9];
    }
    
    transferInProgress = false;
}
```

### 5.2 System Integration Helpers
**File**: `src/ecs/systems/collision_system.h` (new)

**Objective**: Helper functions for systems that consume collision events.

**Interface**:
```cpp
namespace CollisionSystem {
    // Global event manager access
    CollisionEventManager& getEventManager();
    
    // Event filtering helpers
    std::vector<CollisionEvent> getCollisionsForEntity(EntityID entity);
    std::vector<CollisionEvent> getCollisionsByLayer(uint32_t layer);
    std::vector<CollisionEvent> getCollisionsInRegion(glm::vec3 center, float radius);
    
    // Event processing utilities
    void forEachCollision(std::function<void(const CollisionEvent&)> callback);
    void forEntityCollisions(EntityID entity, std::function<void(const CollisionEvent&)> callback);
    
    // Statistics and debugging
    uint32_t getFrameCollisionCount();
    float getAverageCollisionsPerFrame();
    void logCollisionStatistics();
}
```

---

## Phase 6: Entity Factory & Component Integration

### 6.1 Entity Factory Updates
**File**: `src/ecs/entity_factory.h`

**Objective**: Update entity creation to include collision components.

**Modified Factory Methods**:
```cpp
// Add collision support to entity creation
static Entity createEntity(flecs::world& world, 
                          const Transform& transform,
                          const Renderable& renderable, 
                          const MovementPattern& movement,
                          const Collider& collider = Collider{}) {
    auto entity = world.entity()
        .set<Transform>(transform)
        .set<Renderable>(renderable)
        .set<MovementPattern>(movement)
        .set<Collider>(collider)
        .add<GPUUploadPending>();
    
    return Entity{entity};
}

// Specialized methods for collision scenarios
static std::vector<Entity> createSwarmWithCollision(
    flecs::world& world,
    uint32_t count,
    glm::vec3 center,
    float radius,
    float collisionRadius = 0.5f,
    uint32_t collisionLayer = 0
);
```

### 6.2 GPU Entity Manager Updates
**File**: `src/ecs/gpu_entity_manager.cpp`

**Objective**: Update GPU entity upload to include collision data.

**Modified Upload Logic**:
```cpp
GPUEntity GPUEntity::fromECS(const Transform& transform, 
                            const Renderable& renderable, 
                            const MovementPattern& pattern,
                            const Collider& collider) {
    GPUEntity entity{};
    
    // Existing data population...
    // ... movement params, color, runtime state ...
    
    // Populate model matrix as before
    entity.modelMatrix = transform.getMatrix();
    
    // OVERWRITE modelMatrix[3] with collision data
    entity.modelMatrix[3] = glm::vec4(
        collider.radius,                    // x: collision radius
        static_cast<float>(collider.layer), // y: collision layer
        static_cast<float>(collider.mask),  // z: collision mask
        collider.enabled ? 1.0f : 0.0f     // w: enabled flag
    );
    
    return entity;
}
```

---

## Phase 7: Debugging & Validation

### 7.1 Debug Visualization
**File**: `src/ecs/systems/collision_debug_system.h` (new)

**Objective**: Visual debugging of collision detection.

**Features**:
- Draw collision radius circles around entities
- Highlight colliding entity pairs
- Visualize spatial hash grid boundaries
- Real-time collision statistics overlay

### 7.2 Performance Monitoring
**File**: `src/ecs/profiler.h` (extend existing)

**Objective**: Add collision-specific performance metrics.

**New Metrics**:
```cpp
struct CollisionProfilerData {
    float collisionComputeTime;     // GPU time for collision detection
    uint32_t collisionsPerFrame;    // Number of collisions detected
    uint32_t collisionChecksPerFrame; // Total entity pair checks
    float gridOccupancyRatio;       // Percentage of grid cells occupied
    float transferTime;             // GPU→CPU transfer time
    
    void reset();
    void logSummary();
};
```

### 7.3 Unit Tests
**Files**:
- `tests/collision_tests.cpp` (new)
- `tests/spatial_hash_tests.cpp` (new)

**Test Coverage**:
- Spatial hash correctness
- Collision pair generation accuracy  
- Event manager functionality
- Component integration
- Memory buffer management

---

## Phase 8: Optimization & Polish

### 8.1 Performance Optimization
- **Adaptive Grid Sizing**: Dynamic cell size based on entity density
- **Hierarchical Spatial Hashing**: Multi-level grids for mixed entity sizes
- **Temporal Coherence**: Exploit frame-to-frame collision consistency
- **Subgroup Operations**: Use GPU subgroup features where available

### 8.2 Memory Optimization  
- **Dynamic Buffer Resizing**: Adjust collision buffer sizes based on usage
- **Memory Pool Management**: Reuse collision event allocations
- **Staging Buffer Optimization**: Minimize GPU-CPU transfer overhead

### 8.3 Configuration & Tuning
- **Runtime Parameters**: Collision layer configuration, grid size tuning
- **Performance Profiling**: Automated performance regression detection
- **Quality Settings**: Collision detection quality vs performance trade-offs

---

## Implementation Notes

### Critical Dependencies
1. **GPU Compute Support**: Requires Vulkan 1.1+ with compute shader support
2. **Atomic Operations**: GPU must support 32-bit atomic operations efficiently
3. **Memory Bandwidth**: System requires adequate GPU memory bandwidth for hash operations

### Performance Considerations
- **Grid Cell Size**: Should be ~2× average entity collision radius for optimal performance
- **Entity Density**: Performance degrades quadratically beyond ~200K entities in dense clusters  
- **Memory Usage**: Additional ~7MB GPU memory for collision infrastructure

### Validation Requirements
- **Collision Accuracy**: Must detect all valid collisions without false positives
- **Performance Stability**: Collision detection must not cause frame rate drops below 60 FPS
- **Memory Stability**: No memory leaks or buffer overflows under any entity configuration

This implementation plan provides a comprehensive roadmap for adding robust, high-performance GPU-driven collision detection to the Fractalia2 engine while maintaining the existing architecture's performance characteristics and design principles.