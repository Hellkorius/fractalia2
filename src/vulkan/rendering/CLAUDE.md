# Vulkan Rendering Subsystem

## Purpose
Declarative frame graph execution engine that orchestrates GPU compute and graphics workloads with automatic dependency resolution, optimal barrier insertion, and advanced resource management. Core rendering coordinator for 80,000+ entity GPU-driven pipeline.

## File/Folder Hierarchy
```
vulkan/rendering/
├── frame_graph.h                        # FrameGraph core coordinator + FrameGraphNode base
├── frame_graph.cpp                      # Execution engine with cycle detection + memory management
├── frame_graph_resource_registry.h      # ECS buffer import interface
└── frame_graph_resource_registry.cpp    # GPUEntityManager→FrameGraph bridge
```

## Inputs & Outputs (by component)

### FrameGraph (`frame_graph.h/.cpp`)
**Central frame graph execution coordinator with topological sorting, barrier optimization, and GPU monitoring**

**Inputs:**
- **VulkanContext&** - Device/physical device/loader access for Vulkan operations
- **VulkanSync*** - Synchronization primitives (fences/semaphores, NOT command buffers)
- **QueueManager*** - Per-frame command buffer providers: `getComputeCommandBuffer()`, `getGraphicsCommandBuffer()`
- **Monitoring (optional):**
  - `GPUMemoryMonitor*` - Memory pressure detection for resource eviction
  - `GPUTimeoutDetector*` - GPU health monitoring with execution abort capability
- **External Resources:**
  - `importExternalBuffer(name, VkBuffer, size, usage)` - ECS-managed buffers
  - `importExternalImage(name, VkImage, VkImageView, format, extent)` - Render targets
- **Internal Resources:**
  - `createBuffer(name, size, usage)` - Managed buffers with RAII cleanup
  - `createImage(name, format, extent, usage)` - Managed images with RAII cleanup
- **Nodes:** `addNode<NodeType>(args...)` - Template instantiation with automatic ID assignment
- **Frame Data:** `updateFrameData(time, deltaTime, frameCounter, currentFrameIndex)`

**Outputs:**
- **ExecutionResult** - `{computeCommandBufferUsed: bool, graphicsCommandBufferUsed: bool}`
- **Resource Access:** `getBuffer(id)`, `getImage(id)`, `getImageView(id)` for node execution
- **Compiled Command Buffers** - Ready for VulkanRenderer submission
- **Barrier Operations** - Optimal compute→graphics synchronization inserted automatically
- **Memory Management** - Pressure-driven resource eviction and allocation telemetry
- **Error Recovery** - Partial compilation fallback for cyclic dependencies

**Data Structures:**
- `FrameGraphResource = std::variant<FrameGraphBuffer, FrameGraphImage>` - RAII resource union
- `ResourceDependency{resourceId, access, stage}` - Node I/O dependency declaration
- `NodeBarrierInfo` - Per-node barrier batches with optimal async insertion points
- `ResourceCriticality{Critical, Important, Flexible}` - Memory allocation priority classification
- `AllocationTelemetry` - Performance tracking for memory allocation strategies

**Resource Categories:**
- **External** (`isExternal=true`): GPUEntityManager VkBuffers, lifecycle managed externally
- **Internal** (`isExternal=false`): Frame graph creates/destroys with RAII wrappers
- **Critical**: Entity/position buffers requiring device-local memory, fail-fast allocation
- **Evictable**: Non-critical resources with >3s access time for memory pressure relief

### FrameGraphResourceRegistry (`frame_graph_resource_registry.h/.cpp`)
**Standardized ECS buffer import bridge with consistent resource naming**

**Inputs:**
- **FrameGraph*** - Target frame graph for external resource registration
- **GPUEntityManager*** - Source of managed VkBuffer handles:
  - `getEntityBuffer()` - 128-byte GPUEntity structs (hot: movement params, cold: transform matrix)
  - `getPositionBuffer()` - Primary vec4 positions for graphics vertex input
  - `getCurrentPositionBuffer()` - Source positions for compute interpolation
  - `getTargetPositionBuffer()` - Destination positions for compute interpolation
- **Buffer Specifications:**
  - Entity: `maxEntities * sizeof(GPUEntity)` (128 bytes)
  - Positions: `maxEntities * sizeof(glm::vec4)` (16 bytes)

**Outputs:**
- **Resource IDs** for node dependency declarations:
  - `entityBufferId` - "EntityBuffer" import with dual usage flags
  - `positionBufferId` - "PositionBuffer" import with dual usage flags
  - `currentPositionBufferId` - "CurrentPositionBuffer" compute-only storage
  - `targetPositionBufferId` - "TargetPositionBuffer" compute-only storage
- **Usage Flags Applied:**
  - Primary buffers: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`
  - Interpolation buffers: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`

## Data Flow Architecture

**Resource Registration Flow:**
```
GPUEntityManager VkBuffers → FrameGraphResourceRegistry → FrameGraph external imports → Node dependencies
```

**Compilation Flow:**
```
1. Node registration → Dependency graph construction → Topological sort with cycle detection
2. Barrier analysis (O(n)) → Optimal barrier batch creation → Node setup() calls
```

**Execution Flow:**
```
1. Queue requirement analysis → Command buffer begin → Node execution with barriers
2. Timeout monitoring (optional) → Memory pressure checks → Command buffer end
```

**Error Recovery Flow:**
```
Circular dependencies → Detailed cycle analysis → Partial compilation fallback
Memory pressure → Resource eviction → Continued execution
GPU timeout → Health check failure → Execution abort
```

## Peripheral Dependencies

### Core Vulkan Infrastructure
- **vulkan_context.h** - Device/loader/physical device access
- **queue_manager.h** - Per-frame command buffer management (SEPARATE from sync)
- **vulkan_sync.h** - Fence/semaphore primitives (NO command buffer management)
- **vulkan_utils.h** - Memory type resolution via `findMemoryType()`
- **vulkan_raii.h** - RAII wrappers: `Buffer`, `Image`, `DeviceMemory`, `ImageView`

### Render Node Implementations  
- **nodes/entity_compute_node.h** - Movement calculation (`needsComputeQueue()=true`)
- **nodes/entity_graphics_node.h** - Instanced rendering (`needsGraphicsQueue()=true`)
- **FrameGraphNode interface** - `getInputs()`, `getOutputs()`, `execute()` pure virtuals

### ECS Data Sources
- **ecs/gpu/gpu_entity_manager.h** - VkBuffer providers for external import
- **ecs/gpu/entity_buffer_manager.h** - GPUEntity struct definition (128-byte layout)
- **Component system** - Transform/Renderable/MovementPattern → GPUEntity mapping

### Optional Monitoring Integration
- **monitoring/gpu_memory_monitor.h** - `getMemoryPressure()` threshold detection
- **monitoring/gpu_timeout_detector.h** - `isGPUHealthy()`, recovery recommendations

## Key Technical Notes

### Compilation Algorithm
- **Dependency Analysis**: Resource producers mapped O(1) via `getOutputs()`, consumers via `getInputs()`
- **Topological Sort**: Kahn's algorithm with enhanced circular dependency detection
- **Cycle Recovery**: Detailed path analysis with resolution suggestions, partial compilation fallback
- **Barrier Optimization**: Single O(n) pass tracking resource writes, batched insertion at optimal points

### Memory Management Strategy
- **Allocation Criticality**: Device-local preferred, fallback strategies per resource type
- **RAII Lifecycle**: Automatic cleanup via `vulkan_raii` wrappers, `cleanupBeforeContextDestruction()`
- **Pressure Response**: LRU-based resource eviction, allocation telemetry tracking
- **Retry Logic**: Exponential backoff for transient allocation failures

### Execution Optimizations
- **Queue Analysis**: Minimal command buffer allocation based on node requirements
- **Barrier Batching**: Per-node barrier groups, no deduplication for CPU performance
- **Timeout Integration**: Optional GPU health monitoring with execution abort capability
- **Resource Tracking**: Access time monitoring for intelligent eviction candidates

### Error Handling Levels
1. **Compilation Failures**: Transactional state backup/restore, partial execution fallback
2. **Resource Pressure**: Automatic cleanup and eviction, continued execution
3. **GPU Timeouts**: Health monitoring integration, execution abort with telemetry
4. **Critical Allocation Failures**: Fail-fast for entity buffers, performance warnings

### Integration Constraints
- **Command Buffer Lifecycle**: Frame graph assumes reset buffers, returns submit-ready state
- **External Resource Lifecycle**: GPUEntityManager maintains VkBuffer lifecycle, frame graph imports handles only  
- **Monitoring Integration**: Optional dependency, graceful degradation without monitoring
- **Node Execution Order**: Linear execution respecting computed dependencies, no parallel node execution

---
**Implementation Status**: Production-ready. Advanced cycle detection, memory pressure handling, GPU timeout integration, and RAII resource management fully implemented. All external dependencies stable.

**Update Note**: If any referenced external files (nodes/, monitoring/, core/, ecs/gpu/) change their APIs or data structures, this documentation must be updated to reflect the changes.