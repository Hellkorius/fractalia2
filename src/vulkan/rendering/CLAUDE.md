# Vulkan Rendering Subsystem

## Purpose
Frame graph coordination and resource management system. Provides declarative render graph compilation with automatic dependency resolution, synchronization barrier insertion, and resource lifecycle management for Vulkan rendering pipeline.

## File/Folder Hierarchy
```
vulkan/rendering/
├── frame_graph.h               # FrameGraph interface and node base classes
├── frame_graph.cpp             # FrameGraph implementation and node placeholders  
├── frame_graph_resource_registry.h  # Resource import interface
└── frame_graph_resource_registry.cpp # ECS-to-frame-graph resource bridge
```

## Inputs & Outputs (by component)

### FrameGraph (`frame_graph.h/.cpp`)
**Core frame graph coordinator with automatic dependency resolution and barrier optimization**

**Inputs:**
- `VulkanContext*` - Device handle, physical device, Vulkan function loader access
- `VulkanSync*` - Command buffer pools, synchronization primitives (fences/semaphores)
- External resources via `importExternalBuffer()` / `importExternalImage()`
  - `VkBuffer`, `VkImage` handles with usage flags and metadata
- FrameGraphNode instances via `addNode<NodeType>()` template
- Frame data: `time`, `deltaTime`, `frameCounter`, `currentFrameIndex`

**Outputs:**
- Compiled execution graph with topological dependency ordering
- `ExecutionResult` indicating which command buffers were used (compute/graphics)
- Resource handles via `getBuffer()`, `getImage()`, `getImageView()`
- Automatic barrier insertion for compute→graphics resource transitions
- Command buffer recording with proper synchronization points

**Data Structures:**
- `FrameGraphResource` = `std::variant<FrameGraphBuffer, FrameGraphImage>`
- `ResourceDependency` = `{resourceId, access, stage}` for dependency tracking
- `BarrierInfo` = batched buffer/image barriers with pipeline stage masks

**Resource Management:**
- Internal resources: Full lifecycle (create/destroy Vulkan objects)
- External resources: Import-only (lifecycle managed externally, `isExternal=true`)
- Resource ID mapping: String names → numeric IDs for node references

### FrameGraphResourceRegistry (`frame_graph_resource_registry.h/.cpp`)  
**Bridge between ECS system and frame graph resource management**

**Inputs:**
- `FrameGraph*` - Frame graph instance for resource import
- `GPUEntityManager*` - ECS entity buffer provider
  - Entity buffer (`VkBuffer`) - Entity data storage
  - Position buffer (`VkBuffer`) - Primary position data  
  - Current position buffer (`VkBuffer`) - Interpolation source
  - Target position buffer (`VkBuffer`) - Interpolation target

**Outputs:**  
- Resource IDs for imported buffers:
  - `entityBufferId` - Main entity data buffer ID
  - `positionBufferId` - Primary position buffer ID  
  - `currentPositionBufferId` - Current interpolation buffer ID
  - `targetPositionBufferId` - Target interpolation buffer ID
- Resource registration with frame graph naming: "EntityBuffer", "PositionBuffer", etc.

**Buffer Specifications:**
- Entity buffer: `maxEntities * sizeof(GPUEntity)`, usage: `STORAGE_BUFFER | VERTEX_BUFFER`
- Position buffers: `maxEntities * sizeof(glm::vec4)`, usage: `STORAGE_BUFFER [| VERTEX_BUFFER]`

## Peripheral Dependencies

### Core Infrastructure (`../core/`)
- `vulkan_context.h` - Device management, function loader access
- `vulkan_sync.h` - Command buffer pools, fence/semaphore synchronization
- `vulkan_utils.h` - Memory type finding, utility functions

### Render Graph Nodes (`../nodes/`)
- `entity_compute_node.h` - Compute shader node for entity movement
- `entity_graphics_node.h` - Graphics rendering node for entity display  
- Node implementations inherit from `FrameGraphNode` base class

### ECS Bridge (`../../ecs/`)
- `gpu_entity_manager.h` - Entity buffer management, CPU↔GPU data transfer
- Provides VkBuffer handles for entity and position data

## Key Notes

### Compilation Process
1. **Dependency Analysis**: `buildDependencyGraph()` analyzes resource read/write patterns
2. **Topological Sort**: `topologicalSort()` orders nodes respecting dependencies
3. **Barrier Insertion**: `insertSynchronizationBarriers()` adds compute→graphics barriers
4. **Node Setup**: Calls `setup()` on all nodes post-compilation

### Synchronization Strategy  
- **O(n) barrier optimization**: Single-pass barrier insertion with resource write tracking
- **Compute→Graphics barriers only**: Batched buffer/image barriers for queue transitions
- **Duplicate prevention**: Checks existing barriers to avoid redundant synchronization
- **Linear execution**: Single execution order respects all resource dependencies

### Resource Lifecycle
- **External resources**: `isExternal=true`, lifecycle managed by owner (GPUEntityManager)
- **Internal resources**: `isExternal=false`, full Vulkan object lifecycle management  
- **Frame reset**: Removes non-external resources, preserves external buffer imports
- **Name collision prevention**: Unique string names enforced during resource creation

### Node Execution Model
- **Command buffer selection**: Nodes specify `needsComputeQueue()` / `needsGraphicsQueue()`
- **Barrier timing**: Inserted before first graphics node after compute execution
- **Frame data propagation**: `updateFrameData()` distributes timing to all nodes
- **Template-based node creation**: `addNode<NodeType>()` with automatic ID assignment

### Error Handling
- **Initialization guards**: All operations check `initialized` state
- **Resource validation**: Name uniqueness, handle validity, dependency cycles
- **Graceful degradation**: Failed operations return invalid IDs/false status

---
**Update Requirement**: If any referenced files, interfaces, or data structures change, this documentation must be updated to maintain accuracy.