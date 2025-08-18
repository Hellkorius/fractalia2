# Vulkan Render Graph Nodes

## Overview
This directory contains frame graph node implementations that represent discrete GPU operations in the rendering pipeline. Each node declares its resource dependencies and executes Vulkan commands within the frame graph system.

## Architecture

### Node Hierarchy
```
FrameGraphNode (base class)
├── EntityComputeNode     # GPU compute movement calculation
├── EntityGraphicsNode    # Instanced entity rendering
└── SwapchainPresentNode  # Final presentation to screen
```

## Node Implementations

### EntityComputeNode
**Purpose**: GPU-accelerated entity movement computation using compute shaders

**Input Resources**:
- `entityBuffer` (ReadWrite, ComputeShader) - GPUEntity structs with movement parameters
- `currentPositionBuffer` (ReadWrite, ComputeShader) - Current entity positions for interpolation
- `targetPositionBuffer` (ReadWrite, ComputeShader) - Target positions for smooth movement

**Output Resources**:
- `positionBuffer` (Write, ComputeShader) - Final computed positions for graphics pipeline

**Key APIs**:
- `updateFrameData(float time, float deltaTime, uint32_t frameCounter)` - Updates compute shader push constants
- `needsComputeQueue() -> true` - Requires compute queue execution
- Push constants: `ComputePushConstants` (32 bytes) - time, deltaTime, entityCount, frame, entityOffset

**Technical Function**:
- Dispatches compute shader with 64 threads per workgroup
- Implements adaptive workload chunking (max 512 workgroups per dispatch)
- Includes GPU timeout monitoring integration
- Applies memory barriers for compute→graphics synchronization
- Handles 80k+ entities with interpolated random walk movement

### EntityGraphicsNode
**Purpose**: Instanced rendering of entities using GPU-computed positions

**Input Resources**:
- `entityBuffer` (Read, VertexShader) - Entity data for instanced rendering
- `positionBuffer` (Read, VertexShader) - Computed positions from compute node

**Output Resources**:
- `colorTarget` (Write, ColorAttachment) - MSAA framebuffer output

**Key APIs**:
- `setImageIndex(uint32_t imageIndex)` - Sets current swapchain image
- `updateFrameData(float time, float deltaTime, uint32_t frameIndex)` - Updates frame timing
- `setWorld(flecs::world* world)` - Connects to ECS for camera matrices
- `markUniformBufferDirty()` - Forces uniform buffer update
- Push constants: `VertexPushConstants` (12 bytes) - time, deltaTime, entityCount

**Technical Function**:
- Binds vertex buffers: geometry + per-instance entity data
- Updates uniform buffer with view/projection matrices from ECS camera
- Performs instanced indexed drawing with MSAA (2x samples)
- Optimized uniform buffer caching with dirty tracking
- Renders 80k+ entities in single draw call

### SwapchainPresentNode
**Purpose**: Final presentation of rendered frame to display surface

**Input Resources**:
- `colorTarget` (Read, ColorAttachment) - Rendered frame from graphics node

**Output Resources**: None (presents to swapchain)

**Key APIs**:
- `setImageIndex(uint32_t imageIndex)` - Sets target swapchain image
- `needsGraphicsQueue() -> true` - Requires graphics queue execution

**Technical Function**:
- Declares dependency on final color target
- Ensures proper synchronization before presentation
- Presentation logic handled by frame graph execution system
- No command buffer operations (queue-level presentation)

## Data Flow Pipeline

```
EntityComputeNode:
  IN:  entityBuffer (GPUEntity[80k]), currentPos[], targetPos[]
  OUT: positionBuffer (computed positions)
  
EntityGraphicsNode:
  IN:  entityBuffer, positionBuffer, camera matrices
  OUT: colorTarget (MSAA framebuffer)
  
SwapchainPresentNode:
  IN:  colorTarget
  OUT: display presentation
```

## Memory Layout & Performance

### GPUEntity Structure (128 bytes, 2 cache lines)
- **Hot data** (64 bytes): movementParams, runtimeState, color
- **Cold data** (64 bytes): modelMatrix (rarely accessed)

### Execution Characteristics
- **Compute**: 64 threads/workgroup, chunked dispatch for large entity counts
- **Graphics**: Single instanced draw call with vertex + instance buffers
- **Memory barriers**: Compute→Graphics synchronization points
- **MSAA**: 2x multisampling with resolve to swapchain

## Integration Points

### External Dependencies
- `ComputePipelineManager` - Compute pipeline caching and dispatch
- `GraphicsPipelineManager` - Graphics pipeline state management
- `GPUEntityManager` - CPU↔GPU entity data bridge
- `ResourceContext` - Buffer management and descriptor sets
- `VulkanSwapchain` - Presentation surface management
- `GPUTimeoutDetector` - Performance monitoring and safety

### ECS Integration
- Camera matrices from Flecs world via `CameraManager`
- Entity lifecycle managed by CPU ECS systems
- Frame timing and state propagated through push constants