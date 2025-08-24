# Vulkan Rendering System

## Folder Structure

```
src/vulkan/rendering/
├── compilation/                    (Transforms frame graph nodes into executable order with dependency analysis)
│   ├── dependency_graph.h          
│   ├── dependency_graph.cpp        
│   ├── frame_graph_compiler.h      
│   └── frame_graph_compiler.cpp    
├── execution/                      (Manages Vulkan synchronization barriers during frame graph execution)
│   ├── barrier_manager.h           
│   └── barrier_manager.cpp         
├── resources/                      (Handles Vulkan buffer/image allocation with fallback strategies and cleanup)
│   ├── resource_manager.h          
│   └── resource_manager.cpp        
├── frame_graph.h                   
├── frame_graph.cpp                 
├── frame_graph_debug.h             
├── frame_graph_node_base.h         
├── frame_graph_resource_registry.h 
├── frame_graph_resource_registry.cpp
└── frame_graph_types.h             
```

## File Descriptions

### compilation/dependency_graph.h
**Inputs:** FrameGraph nodes with input/output resource dependencies.  
**Outputs:** GraphData structure containing resource producer mappings and adjacency lists for topological sorting.  
**Purpose:** Builds optimized dependency graphs for O(1) resource producer lookups during compilation.

### compilation/dependency_graph.cpp
**Inputs:** Map of NodeId->FrameGraphNode pairs with resource dependencies.  
**Outputs:** Complete dependency graph with adjacency lists and in-degree tracking.  
**Purpose:** Constructs directed acyclic graphs from resource producer-consumer relationships between nodes.

### compilation/frame_graph_compiler.h
**Inputs:** Frame graph nodes and their resource dependencies.  
**Outputs:** Topologically sorted execution order with circular dependency analysis.  
**Purpose:** Compiles frame graphs into executable order with cycle detection and partial compilation fallback.

### compilation/frame_graph_compiler.cpp
**Inputs:** Node dependency graphs and resource access patterns.  
**Outputs:** Validated execution order or detailed cycle analysis with resolution suggestions.  
**Purpose:** Implements Kahn's algorithm with enhanced cycle detection and generates actionable dependency resolution strategies.

### execution/barrier_manager.h
**Inputs:** Execution order and node resource access patterns.  
**Outputs:** Optimal Vulkan memory barriers for compute-graphics synchronization.  
**Purpose:** Analyzes resource dependencies to insert minimal barriers at optimal points for async execution.

### execution/barrier_manager.cpp
**Inputs:** Resource write tracking and node pipeline stage information.  
**Outputs:** VkBufferMemoryBarrier and VkImageMemoryBarrier batches inserted into command buffers.  
**Purpose:** Creates optimized barriers using hazard analysis that eliminates redundant read-after-write barriers for compute-to-compute transitions, reducing synchronization overhead while maintaining memory safety.

### resources/resource_manager.h
**Inputs:** Resource creation requests with size, format, and usage specifications.  
**Outputs:** Vulkan buffers/images with RAII wrappers and memory pressure tracking.  
**Purpose:** Manages Vulkan resource allocation with criticality-based strategies and memory pressure handling.

### resources/resource_manager.cpp
**Inputs:** Buffer/image specifications and external Vulkan handles for import.  
**Outputs:** Allocated Vulkan resources with fallback memory strategies and eviction candidates.  
**Purpose:** Implements robust allocation with device/host memory fallbacks and performs automatic cleanup under memory pressure.

### frame_graph.h
**Inputs:** Vulkan context, sync objects, and queue managers for initialization.  
**Outputs:** Compiled frame graph with resource handles and execution coordination.  
**Purpose:** Main coordinator orchestrating modular compilation, barrier management, and resource allocation components.

### frame_graph.cpp
**Inputs:** Frame timing data and node execution parameters.  
**Outputs:** Command buffer recordings with optimal barriers and resource transitions.  
**Purpose:** Executes compiled frame graphs with timeout monitoring and delegates resource management to specialized components.

### frame_graph_node_base.h
**Inputs:** Node identification and resource dependency specifications.  
**Outputs:** Streamlined lifecycle with execute(commandBuffer, frameGraph, time, deltaTime) and optional onFirstUse() hook.  
**Purpose:** Base class defining simplified node interface - reduced from 4 lifecycle methods to 2 essential methods with timing parameters moved into execute().

### frame_graph_resource_registry.h
**Inputs:** FrameGraph and GPUEntityManager references for resource import.  
**Outputs:** Resource IDs for entity buffers imported into frame graph system.  
**Purpose:** Bridges ECS GPU buffers into frame graph resource management for unified access.

### frame_graph_resource_registry.cpp
**Inputs:** External entity and position buffer handles from GPU entity manager.  
**Outputs:** Imported frame graph resource IDs for entity rendering pipeline.  
**Purpose:** Imports ECS-managed buffers as external resources while preserving their lifecycle management.

### frame_graph_types.h
**Inputs:** Type requirements for resource and node identification.  
**Outputs:** Unified type definitions for ResourceId, NodeId, dependency descriptors, and NodePushConstants struct.  
**Purpose:** Defines core types for resource access patterns, pipeline stages, and standardized NodePushConstants with flexible param1/param2 fields for unified compute shader parameters across all frame graph nodes.

### frame_graph_debug.h
**Inputs:** Debug build configuration and frame graph execution context.  
**Outputs:** Zero-overhead debug utilities with throttled logging, consolidated logNodeExecution() and logChunkedExecution() functions.  
**Purpose:** Eliminates duplicate debug code across compute nodes with compile-time optimization - provides unified debug patterns with no-op implementations in release builds.