# ECS GPU Bridge Module

## Purpose
CPU-GPU bridge for high-performance entity rendering. Transforms ECS components into GPU-optimized structures, manages Vulkan buffers, and coordinates compute/graphics pipeline data flow for 80,000+ entities at 60 FPS.

## File Hierarchy
```
src/ecs/gpu/
├── gpu_entity_manager.h/.cpp          # Main orchestrator - CPU→GPU entity management
├── entity_buffer_manager.h/.cpp       # Vulkan buffer management with ping-pong
├── entity_descriptor_manager.h/.cpp   # Descriptor set management for pipelines
└── CLAUDE.md                          # This documentation
```

## Inputs & Outputs

### GPUEntityManager (Main Bridge)
**Inputs:**
- `flecs::entity` vectors from ECS systems
- ECS components: `Transform`, `Renderable`, `MovementPattern` 
- Vulkan dependencies: `VulkanContext`, `VulkanSync`, `ResourceContext`
- Max entity count (default: 131,072)

**Outputs:**
- `VkBuffer` handles for entity/position data
- `GPUEntity` structs (128-byte cache-optimized)
- Upload coordination for compute/graphics pipelines
- Descriptor set access via `EntityDescriptorManager`

**Key Methods:**
- `addEntitiesFromECS()` - Converts ECS entities to `GPUEntity` format
- `uploadPendingEntities()` - Batch upload to GPU buffers
- `getEntityBuffer()/getPositionBuffer()` - Direct buffer access for frame graph

### EntityBufferManager (Buffer Management)
**Inputs:**
- Buffer sizes based on max entities (128 bytes × count for entities, 16 bytes × count for positions)
- Raw data for upload via `copyDataToBuffer()`
- Frame indices for ping-pong buffer access

**Outputs:**
- 5 Vulkan buffers: entity, position, positionAlternate, currentPosition, targetPosition
- Ping-pong buffer access: `getComputeWriteBuffer(frameIndex)`, `getGraphicsReadBuffer(frameIndex)`
- Buffer properties: sizes, handles, memory objects

**Buffer Layout:**
- Entity buffer: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT`
- Position buffers: Same usage flags, enable async compute/graphics operations

### EntityDescriptorManager (Pipeline Interface)
**Inputs:**
- Buffer handles from `EntityBufferManager`
- Pipeline layout requirements (compute: 4 storage buffers, graphics: 1 uniform + 2 storage)

**Outputs:**
- Descriptor set layouts for compute and graphics pipelines
- Populated descriptor sets with buffer bindings
- Swapchain recreation support via `recreateComputeDescriptorSets()`

**Pipeline Bindings:**
- Compute: Entity buffer, position input/output, additional storage buffers
- Graphics: Uniform buffer, entity buffer, position buffer

### GPUEntity Structure (Data Format)
**Input Transform:** ECS components → 128-byte GPU structure
```cpp
// Cache Line 1 (0-63): HOT DATA for compute shaders
glm::vec4 velocity;           // velocity.xy, damping (0.001), reserved
glm::vec4 movementParams;     // amplitude, frequency, phase, timeOffset
glm::vec4 runtimeState;       // totalTime, initialized, stateTimer, entityState  
glm::vec4 color;              // RGBA from Renderable

// Cache Line 2 (64-127): COLD DATA
glm::mat4 modelMatrix;        // Transform matrix
```

**Conversion Logic:** `GPUEntity::fromECS()` with thread-local RNG for stateTimer randomization

## Peripheral Dependencies

### Upstream (Data Sources)
- **ECS Components** (`../components/`): Source data structures
- **Flecs Systems** (`../systems/`): Entity queries and component access
- **Services** (`../services/`): High-level coordination via RenderingService

### Downstream (Data Consumers)  
- **Vulkan Compute Nodes** (`../../vulkan/nodes/entity_compute_node.*`): Consumes entity/position buffers for movement calculations
- **Vulkan Graphics Nodes** (`../../vulkan/nodes/entity_graphics_node.*`): Consumes buffers for instanced rendering
- **Frame Graph** (`../../vulkan/rendering/`): Resource management and execution ordering

### Core Dependencies
- **VulkanContext**: Device, loader, validation  
- **ResourceContext**: Memory allocation and buffer creation
- **VulkanSync**: Synchronization primitives for GPU operations

## Key Notes

### Performance Optimizations
- **Cache-Line Optimization**: GPUEntity uses 2 cache lines (hot/cold data separation)
- **Ping-Pong Buffers**: Async compute/graphics with `getComputeWriteBuffer()/getGraphicsReadBuffer()`
- **Batch Upload**: Staging vector accumulates entities before GPU transfer
- **Thread-Local RNG**: Optimized random state generation for movement staggering

### Vulkan Integration Pattern
- **Local Caching**: `const auto& vk = context->getLoader(); VkDevice device = context->getDevice();`
- **RAII Resource Management**: All buffers have paired memory objects with proper cleanup
- **Descriptor Recreation**: Critical for swapchain resize events

### Data Flow
1. **ECS → Staging**: `addEntitiesFromECS()` converts components to GPUEntity format
2. **Staging → GPU**: `uploadPendingEntities()` batch uploads to Vulkan buffers  
3. **Compute Pipeline**: Reads entity buffer, writes to ping-pong position buffers
4. **Graphics Pipeline**: Reads entity + position buffers for instanced rendering
5. **Buffer Rotation**: Frame-based ping-pong prevents read/write conflicts

### Critical Dependencies
- Buffer manager must initialize before descriptor manager
- Descriptor sets require buffer handles for binding updates
- Frame graph nodes depend on stable buffer handles throughout frame execution
- Swapchain recreation requires descriptor set regeneration

### Edge Cases
- **Empty Entity Upload**: Handles zero-entity case without GPU operations
- **Buffer Overflow**: Enforces MAX_ENTITIES limit (131,072) with staging validation
- **Initialization Order**: Dependencies validated during initialize() calls
- **Cleanup Order**: Reverse dependency cleanup prevents use-after-free

---
**Note**: This file must be updated when GPUEntity structure, buffer layouts, or pipeline integration points change.