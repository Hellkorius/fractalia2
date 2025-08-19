# Vulkan Rendering Subsystem

## Purpose
Declarative frame graph execution coordinator with automatic dependency resolution, barrier optimization, and resource lifecycle management. Orchestrates compute→graphics pipeline transitions with O(n) barrier complexity and integrates GPU memory pressure monitoring.

## File/Folder Hierarchy
```
vulkan/rendering/
├── frame_graph.h               # FrameGraph coordinator and FrameGraphNode base class
├── frame_graph.cpp             # Execution engine with advanced cycle detection
├── frame_graph_resource_registry.h  # ECS buffer import interface
└── frame_graph_resource_registry.cpp # GPUEntityManager→FrameGraph bridge
```

## Inputs & Outputs (by component)

### FrameGraph (`frame_graph.h/.cpp`)
**Execution coordinator with topological sorting, barrier optimization, and GPU monitoring integration**

**Inputs:**
- `VulkanContext&` - Device, physical device, loader access
- `VulkanSync*` - Synchronization primitives (no command buffers)
- `QueueManager*` - Per-frame command buffer access
- Optional monitoring: `GPUMemoryMonitor*`, `GPUTimeoutDetector*`
- External resources: `importExternalBuffer()` / `importExternalImage()` with VkBuffer/VkImage handles
- Nodes: `addNode<NodeType>()` template instantiation
- Frame timing: `updateFrameData(time, deltaTime, frameCounter, currentFrameIndex)`

**Outputs:**
- `ExecutionResult{computeCommandBufferUsed, graphicsCommandBufferUsed}` 
- Resource access: `getBuffer()`, `getImage()`, `getImageView()` for node execution
- Optimized barrier batches inserted before graphics nodes requiring compute output
- Memory pressure response: resource eviction and cleanup operations
- Timeout-aware execution with GPU health monitoring integration

**Core Data Types:**
- `FrameGraphResource = std::variant<FrameGraphBuffer, FrameGraphImage>`
- `FrameGraphBuffer/Image` - RAII wrappers with `vulkan_raii::Buffer/Image/Memory`
- `ResourceDependency{resourceId, access, stage}` - Node input/output declarations
- `NodeBarrierInfo` - Per-node barrier batches for optimal async execution
- `ResourceCriticality{Critical, Important, Flexible}` - Memory allocation strategies

**Resource Classification:**
- External (`isExternal=true`): ECS buffers, lifecycle managed by GPUEntityManager
- Internal (`isExternal=false`): Frame graph creates/destroys Vulkan objects
- Critical resources: Entity/position buffers requiring device-local memory
- Evictable resources: Non-critical with >3s access time for memory pressure relief

### FrameGraphResourceRegistry (`frame_graph_resource_registry.h/.cpp`)  
**ECS buffer import bridge with standardized resource naming**

**Inputs:**
- `FrameGraph*` - Target frame graph for resource import
- `GPUEntityManager*` - Source of ECS-managed VkBuffer handles
  - `getEntityBuffer()` - 128-byte GPUEntity structs (movement params + transform)
  - `getPositionBuffer()` - Primary vec4 positions for graphics rendering
  - `getCurrentPositionBuffer()` - Source positions for compute interpolation
  - `getTargetPositionBuffer()` - Target positions for compute interpolation

**Outputs:**  
- `FrameGraphTypes::ResourceId` handles for node dependency declarations:
  - `entityBufferId` - "EntityBuffer" import
  - `positionBufferId` - "PositionBuffer" import
  - `currentPositionBufferId` - "CurrentPositionBuffer" import
  - `targetPositionBufferId` - "TargetPositionBuffer" import

**Buffer Import Specifications:**
- Entity buffer: `maxEntities * sizeof(GPUEntity)` (128 bytes), `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`
- Position buffer: `maxEntities * sizeof(glm::vec4)`, `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`
- Interpolation buffers: `maxEntities * sizeof(glm::vec4)`, `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`

## External Dependencies

### Core Infrastructure
- `../core/vulkan_context.h` - Device, physical device, loader access
- `../core/queue_manager.h` - Per-frame command buffer management (compute/graphics)
- `../core/vulkan_sync.h` - Fence/semaphore synchronization (no command buffers)
- `../core/vulkan_utils.h` - Memory type resolution utilities
- `../core/vulkan_raii.h` - RAII resource wrappers for automatic cleanup

### Render Nodes
- `../nodes/entity_compute_node.h` - Movement computation (needsComputeQueue=true)
- `../nodes/entity_graphics_node.h` - Instanced rendering (needsGraphicsQueue=true)
- Nodes inherit `FrameGraphNode` with `getInputs()/getOutputs()` dependency declarations

### ECS Data Source
- `../../ecs/gpu_entity_manager.h` - VkBuffer providers for external resource import
- `../../ecs/component.h` - GPUEntity struct (128-byte cache-optimized layout)

### Monitoring Integration
- `../monitoring/gpu_memory_monitor.h` - Memory pressure detection
- `../monitoring/gpu_timeout_detector.h` - GPU health monitoring and workload recommendations

## Technical Architecture

### Compilation Pipeline
1. **Node Registration**: `addNode<NodeType>()` with automatic ID assignment
2. **Dependency Graph**: Resource producers mapped via `getOutputs()`, consumers via `getInputs()`
3. **Topological Sort**: Kahn's algorithm with circular dependency detection and detailed cycle reporting
4. **Barrier Analysis**: Single O(n) pass tracking resource writes for compute→graphics transitions
5. **Node Setup**: Post-compilation `setup()` calls with frame graph context

### Execution Flow
1. **Queue Analysis**: Determine compute/graphics command buffer requirements
2. **Command Begin**: `QueueManager` provides per-frame command buffers
3. **Node Execution**: Linear order with barrier insertion before graphics nodes
4. **Timeout Monitoring**: Optional GPU health checks with workload recommendations
5. **Command End**: Submit-ready command buffers returned to caller

### Barrier Optimization
- **O(n) complexity**: Resource write tracking eliminates O(n²) barrier analysis
- **Batched insertion**: Per-node barrier batches for optimal async execution
- **Compute→Graphics only**: Queue family transitions, no graphics→graphics barriers
- **No deduplication**: Removed hash-based deduplication for CPU performance

### Memory Management
- **Allocation strategies**: Resource criticality determines device-local vs fallback memory
- **RAII lifecycle**: `vulkan_raii::Buffer/Image/Memory` wrappers prevent leaks
- **Pressure response**: Memory monitor integration with automatic resource eviction
- **Retry mechanisms**: Exponential backoff for transient allocation failures

### Advanced Features
- **Cycle detection**: Enhanced analysis with resolution suggestions and partial compilation fallback
- **Resource eviction**: LRU-based cleanup for non-critical resources during memory pressure
- **Timeout integration**: GPU health monitoring with execution abort on unhealthy GPU state
- **Transactional compilation**: State backup/restore on compilation failures

## Integration Points

**Upstream Data Flow:**
- ECS components (Transform, Renderable, MovementPattern) → GPUEntity structs → VkBuffer
- GPUEntityManager provides external buffer handles → FrameGraphResourceRegistry imports
- Frame timing data → updateFrameData() → node execution parameters

**Downstream Execution:**
- Compiled execution order → QueueManager command buffers → node execution
- Resource barriers → optimal compute→graphics synchronization
- ExecutionResult → VulkanRenderer submission coordination

**Error Recovery:**
- Memory pressure → resource eviction → continued execution
- GPU timeouts → execution abort → external workload reduction
- Circular dependencies → partial compilation → degraded but functional execution

---
**Implementation Status**: Production-ready with advanced cycle detection, memory pressure handling, and GPU health monitoring. All external dependencies stable.