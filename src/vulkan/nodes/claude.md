# Vulkan Render Graph Nodes

Frame graph nodes for discrete GPU operations. Each declares resource dependencies and executes Vulkan commands.

## Files
- `entity_compute_node.{h,cpp}` - GPU movement computation
- `entity_graphics_node.{h,cpp}` - Instanced entity rendering  
- `swapchain_present_node.{h,cpp}` - Display presentation
- `claude.md` - Technical documentation and API reference
- `changelog.md` - Development history and improvements

## Node Hierarchy
```
EntityComputeNode     # GPU compute movement (80k entities)
EntityGraphicsNode    # Instanced rendering with MSAA
SwapchainPresentNode  # Display presentation
```

## EntityComputeNode (`entity_compute_node.{h,cpp}`)
**Function**: GPU movement computation with interpolated random walk

**Key Methods**:
- `EntityComputeNode(entityBuffer, positionBuffer, currentPos, targetPos, computeManager, gpuEntityManager, timeoutDetector)`
- `updateFrameData(time, deltaTime, frameCounter)` - Updates push constants
- `execute(commandBuffer, frameGraph)` - Dispatches compute shader
- `executeChunkedDispatch()` - Internal method for large workload chunking

**I/O**:
- IN: `entityBuffer` (R/W), `currentPositionBuffer` (R/W), `targetPositionBuffer` (R/W)
- OUT: `positionBuffer` (Write)
- Push constants: `ComputePushConstants` (32 bytes: time, deltaTime, entityCount, frame, entityOffset)

**Implementation**: 64 threads/workgroup, adaptive chunking (max 512), GPU timeout monitoring, compute→graphics barriers

## EntityGraphicsNode (`entity_graphics_node.{h,cpp}`)
**Function**: Instanced entity rendering from computed positions

**Key Methods**:
- `EntityGraphicsNode(entityBuffer, positionBuffer, colorTarget, graphicsManager, swapchain, resourceContext, gpuEntityManager)`
- `setImageIndex(imageIndex)` - Sets swapchain target
- `updateFrameData(time, deltaTime, frameIndex)` - Updates timing
- `setWorld(world)` - Links to ECS for camera
- `markUniformBufferDirty()` - Forces uniform update
- `execute(commandBuffer, frameGraph)` - Renders entities
- `getCameraMatrices()` - Internal camera matrix retrieval
- `updateUniformBufferData()` - Internal uniform buffer management

**I/O**:
- IN: `entityBuffer` (Read), `positionBuffer` (Read), camera matrices from ECS
- OUT: `colorTarget` (Write, MSAA 2x)  
- Push constants: `VertexPushConstants` (12 bytes: time, deltaTime, entityCount)

**Implementation**: Single instanced draw call, uniform buffer caching with dirty tracking, vertex+instance buffers

## SwapchainPresentNode (`swapchain_present_node.{h,cpp}`)
**Function**: Present rendered frame to display

**Key Methods**:
- `SwapchainPresentNode(colorTarget, swapchain)`
- `setImageIndex(imageIndex)` - Sets presentation target
- `getImageIndex()` - Gets current image index for debugging
- `execute(commandBuffer, frameGraph)` - Declares dependencies only

**I/O**:
- IN: `colorTarget` (Read)
- OUT: swapchain presentation

**Implementation**: Dependency declaration only, actual presentation handled at queue level

## Data Flow
```
Compute: entityBuffer[80k] + positions → positionBuffer
Graphics: entityBuffer + positionBuffer + camera → colorTarget  
Present: colorTarget → display
```

## Dependencies
- `ComputePipelineManager` - Compute pipeline caching
- `GraphicsPipelineManager` - Graphics pipeline states
- `GPUEntityManager` - CPU↔GPU entity bridge  
- `ResourceContext` - Buffer/descriptor management
- `VulkanSwapchain` - Presentation surface
- `GPUTimeoutDetector` - Performance monitoring
- `FrameGraph` - Resource scheduling system

## Performance Notes
- **GPUEntity**: 128 bytes (hot: 64B movement/color, cold: 64B matrix)
- **Execution**: Compute chunked dispatch, graphics single draw call, 2x MSAA