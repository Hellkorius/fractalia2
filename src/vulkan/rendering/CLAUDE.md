# Vulkan Rendering - Frame Graph System

## Purpose
Declarative frame graph execution engine that orchestrates GPU compute and graphics workloads with automatic dependency resolution, optimal barrier insertion, and resource lifecycle management. Central coordinator for 80,000+ entity GPU-driven rendering pipeline.

## Architecture Overview

### Core Components
```
rendering/
├── frame_graph.h/cpp              # Main coordinator - node execution & resource access
├── frame_graph_types.h            # Type definitions & enums
├── frame_graph_node_base.h        # Node interface & dependency declarations
├── frame_graph_resource_registry.* # ECS buffer import bridge
├── compilation/                   # Dependency analysis & execution ordering
│   ├── dependency_graph.*         # Topological sorting & cycle detection
│   └── frame_graph_compiler.*     # Compilation orchestration & error recovery
├── execution/                     # Runtime execution coordination
│   └── barrier_manager.*          # Memory barrier optimization & insertion
└── resources/                     # Resource lifecycle management
    └── resource_manager.*         # RAII buffers/images & memory pressure handling
```

### Data Flow
```
1. Registration: Nodes → Resources → Dependencies
2. Compilation: Dependency graph → Topological sort → Barrier analysis
3. Execution: Command buffer setup → Node execution → Barrier insertion
```

## Key Classes & Interfaces

### FrameGraph (Main Coordinator)
**Responsibilities**: Node execution orchestration, resource access, lifecycle management

**Key Methods**:
- `addNode<T>()` - Template node registration with automatic dependency tracking
- `createBuffer/Image()` - Managed resource creation with RAII cleanup
- `importExternalBuffer/Image()` - ECS buffer integration (external lifecycle)
- `compile()` - Dependency resolution with cycle detection & recovery
- `execute()` - Command buffer generation with optimal barrier placement

**Resource Categories**:
- **External**: ECS-managed VkBuffers (entity/position data) - handle imports only
- **Internal**: Frame graph-owned resources with automatic RAII cleanup
- **Critical**: Device-local required (entity buffers) - fail-fast allocation
- **Evictable**: Memory pressure candidates with LRU-based cleanup

### FrameGraphNode (Base Interface)
**Pure Virtual Interface** for render operations:
```cpp
virtual std::vector<ResourceDependency> getInputs() = 0;    // Read dependencies
virtual std::vector<ResourceDependency> getOutputs() = 0;   // Write dependencies  
virtual bool needsComputeQueue() = 0;                       // Queue requirements
virtual bool needsGraphicsQueue() = 0;
virtual void setup(FrameGraph& frameGraph) = 0;             // Pre-execution setup
virtual void execute(VkCommandBuffer cmd, FrameGraph& frameGraph) = 0;
```

### ResourceManager
**RAII Resource Lifecycle** with memory pressure handling:
- `FrameGraphBuffer/Image` - vulkan_raii wrappers with automatic cleanup
- Device-local allocation with fallback strategies per criticality
- LRU-based eviction on memory pressure (>3s access time threshold)
- Allocation telemetry for performance optimization

### FrameGraphCompiler  
**Dependency Resolution Engine**:
- Kahn's algorithm topological sorting with enhanced cycle detection
- Detailed circular dependency analysis with resolution suggestions
- Partial compilation fallback for robust error recovery
- Transactional state backup/restore for compilation safety

### BarrierManager
**Memory Synchronization Optimization**:
- Single O(n) pass resource write tracking across all nodes
- Per-node barrier batching for optimal async execution points
- Automatic compute→graphics synchronization insertion
- Pipeline stage optimization (COMPUTE_SHADER → VERTEX_SHADER/VERTEX_INPUT)

## Integration Patterns

### ECS Buffer Import (via FrameGraphResourceRegistry)
```cpp
// Standard ECS resource registration
resourceRegistry.importECSBuffers(frameGraph, gpuEntityManager);
// Results in: "EntityBuffer", "PositionBuffer", "CurrentPositionBuffer", "TargetPositionBuffer"
```

### Node Dependency Declaration
```cpp
std::vector<ResourceDependency> getInputs() override {
    return {{entityBufferId, ResourceAccess::Read, PipelineStage::ComputeShader}};
}
std::vector<ResourceDependency> getOutputs() override {
    return {{positionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader}};
}
```

### Resource Access in Nodes
```cpp
void execute(VkCommandBuffer cmd, FrameGraph& frameGraph) override {
    VkBuffer entityBuffer = frameGraph.getBuffer(entityBufferId);
    VkBuffer positionBuffer = frameGraph.getBuffer(positionBufferId);
    // Use buffers in compute/graphics operations
}
```

## Performance Characteristics

### Compilation (O(V + E))
- **Dependency Analysis**: O(1) producer mapping, O(n) consumer iteration
- **Topological Sort**: O(V + E) via Kahn's algorithm with cycle detection
- **Barrier Analysis**: Single O(n) pass, no deduplication for CPU performance

### Memory Management
- **Pressure Detection**: Optional GPUMemoryMonitor integration
- **Eviction Strategy**: LRU-based with access time tracking
- **Allocation**: Device-local preferred, intelligent fallback per criticality
- **Cleanup**: RAII with `cleanupBeforeContextDestruction()` ordering

### Error Recovery
- **Circular Dependencies**: Detailed cycle analysis → partial compilation fallback
- **Memory Pressure**: Automatic eviction → continued execution
- **GPU Timeouts**: Optional detector integration → execution abort
- **Critical Failures**: Fail-fast entity buffer allocation with performance warnings

## External Dependencies

### Required
- **VulkanContext**: Device/loader/physical device access
- **QueueManager**: Per-frame command buffer provision (separate from sync)
- **VulkanSync**: Fence/semaphore primitives (NO command buffer management)
- **vulkan_raii**: RAII wrappers for automatic resource cleanup

### Optional Monitoring
- **GPUMemoryMonitor**: Memory pressure detection for eviction triggers
- **GPUTimeoutDetector**: GPU health monitoring with execution abort capability

### Node Implementations
- **nodes/entity_compute_node**: Movement computation (needsComputeQueue=true)
- **nodes/entity_graphics_node**: Instanced rendering (needsGraphicsQueue=true)

---
**Status**: Production-ready. Handles 80,000+ entities with automatic dependency resolution, memory pressure management, and robust error recovery. All external integrations stable.