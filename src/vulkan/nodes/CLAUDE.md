# Vulkan Render Graph Nodes

## Purpose
This directory contains the concrete implementation of frame graph nodes that execute specific stages of the Fractalia2 rendering pipeline. Each node represents a discrete GPU operation (compute dispatch, graphics rendering, or presentation) with well-defined input/output dependencies.

## File Hierarchy
```
nodes/
├── CLAUDE.md                    # This documentation
├── changelog.md                 # Recent code quality improvements (2025-01-18)
├── entity_compute_node.h        # Compute movement calculation header
├── entity_compute_node.cpp      # Compute movement implementation with chunked dispatch
├── entity_graphics_node.h       # Graphics rendering header
├── entity_graphics_node.cpp     # Graphics rendering with instanced draw calls
├── swapchain_present_node.h     # Presentation header
└── swapchain_present_node.cpp   # Presentation to swapchain implementation
```

## Input/Output Documentation

### EntityComputeNode (`entity_compute_node.{h,cpp}`)
**Purpose**: GPU compute dispatch for entity movement calculations with random walk algorithm

**Inputs**:
- `entityBufferId` (FrameGraphTypes::ResourceId): Buffer containing GPUEntity structs (128-byte cache-optimized layout)
- `positionBufferId` (FrameGraphTypes::ResourceId): Current position buffer (vec3 positions)
- `currentPositionBufferId` (FrameGraphTypes::ResourceId): Current frame positions
- `targetPositionBufferId` (FrameGraphTypes::ResourceId): Target interpolation positions

**Dependencies**:
- `ComputePipelineManager*`: Compute pipeline caching and dispatch optimization
- `GPUEntityManager*`: CPU-GPU entity data bridge and descriptor set management
- `GPUTimeoutDetector` (optional): GPU timeout detection for stability

**Outputs**:
- Updated position buffers with computed movement for 80,000+ entities
- Chunked compute dispatches (64 threads/workgroup, 512 workgroups max per chunk)

**Data Flow**:
1. Reads GPUEntity movement parameters (amplitude, frequency, phase, center)
2. Executes compute shader with 600-frame interpolated random walk cycles
3. Writes new positions to position buffers with smooth interpolation
4. Uses adaptive dispatch chunking to prevent GPU timeouts

**Key APIs**:
- `updateFrameData(float time, float deltaTime, uint32_t frameCounter)`: Frame state updates
- `execute(VkCommandBuffer, const FrameGraph&)`: Main compute dispatch execution
- Push constants: `ComputePushConstants` (time, deltaTime, entityCount, frame, entityOffset)

### EntityGraphicsNode (`entity_graphics_node.{h,cpp}`)
**Purpose**: GPU graphics rendering of entities with instanced draw calls

**Inputs**:
- `entityBufferId` (FrameGraphTypes::ResourceId): GPUEntity buffer for color/transform data
- `positionBufferId` (FrameGraphTypes::ResourceId): Computed positions from EntityComputeNode
- ECS camera matrices via `flecs::world*` reference

**Dependencies**:
- `GraphicsPipelineManager*`: Graphics pipeline state objects with LRU caching
- `VulkanSwapchain*`: Swapchain extent, framebuffers, render pass management
- `ResourceContext*`: Memory and buffer management
- `GPUEntityManager*`: Entity count and descriptor set access

**Outputs**:
- `colorTargetId` (FrameGraphTypes::ResourceId): Rendered color attachment (MSAA + resolve)
- Single instanced draw call for all entities with MSAA enabled

**Data Flow**:
1. Retrieves camera view/projection matrices from ECS world
2. Updates uniform buffer with cached UBO (dirty tracking optimization)
3. Begins render pass with MSAA color attachments
4. Binds graphics pipeline and descriptor sets
5. Executes single instanced draw call for all entities
6. Ends render pass with automatic MSAA resolve

**Key APIs**:
- `setImageIndex(uint32_t)`: Set swapchain image index for current frame
- `updateFrameData(float time, float deltaTime, uint32_t frameIndex)`: Frame timing data
- `setWorld(flecs::world*)`: ECS world reference for camera matrix access
- `markUniformBufferDirty()`: Force uniform buffer update on camera changes

### SwapchainPresentNode (`swapchain_present_node.{h,cpp}`)
**Purpose**: Final presentation of rendered frame to swapchain

**Inputs**:
- `colorTargetId` (FrameGraphTypes::ResourceId): Rendered color attachment from EntityGraphicsNode

**Dependencies**:
- `VulkanSwapchain*`: Swapchain management and presentation queue access

**Outputs**:
- Presentation to OS window system (no frame graph resources produced)
- Swapchain image transition for next frame acquisition

**Data Flow**:
1. Validates swapchain dependencies and image index bounds
2. Transitions color target to present layout
3. No explicit copy operation (framebuffer already targets swapchain image)
4. Prepares swapchain image for presentation queue submission

**Key APIs**:
- `setImageIndex(uint32_t)`: Current swapchain image index
- `getImageIndex() const`: Current image index (debugging/validation)

## Peripheral Dependencies

### Core Vulkan Infrastructure
- `../core/vulkan_context.*`: Vulkan instance, device, queue management
- `../core/vulkan_swapchain.*`: Swapchain, MSAA, framebuffers
- `../core/vulkan_function_loader.*`: Vulkan function loading
- `../core/vulkan_sync.*`: Fences, semaphores for synchronization

### Pipeline Management Systems
- `../pipelines/compute_pipeline_manager.*`: Compute pipeline caching with dispatch optimization
- `../pipelines/graphics_pipeline_manager.*`: Graphics pipeline state objects with LRU caching
- `../pipelines/descriptor_layout_manager.*`: Descriptor set layout caching and pool management
- `../pipelines/shader_manager.*`: SPIR-V loading, compilation, hot-reload

### Resource Management
- `../resources/resource_context.*`: Buffer and memory management
- `../rendering/frame_graph.*`: Render graph coordinator and resource dependency tracking
- `../rendering/frame_graph_resource_registry.*`: Resource registration with frame graph

### ECS Integration
- `../../ecs/gpu_entity_manager.*`: CPU-GPU bridge for entity data transfer
- `../../ecs/components/`: ECS component definitions (Transform, Renderable, MovementPattern)
- `../../ecs/systems/camera_system.*`: Camera matrix computation
- `flecs.h`: External ECS library for entity lifecycle management

### Performance Monitoring
- `../monitoring/gpu_timeout_detector.*`: GPU timeout detection and recovery
- `../monitoring/compute_stress_tester.*`: Compute workload stress testing

## Key Notes

### Memory Layout Optimization
- GPUEntity struct: 128-byte cache-optimized layout (2 cache lines)
- Hot data (0-63 bytes): movement parameters, runtime state, color
- Cold data (64-127 bytes): model matrix (rarely accessed)

### Performance Characteristics
- Entity capacity: 80,000+ entities at 60 FPS
- Compute dispatch: 64 threads per workgroup, chunked to prevent timeouts
- Graphics rendering: Single instanced draw call with MSAA
- Memory barriers: Linear frame graph insertion, optimized synchronization

### Error Handling Patterns
- Constructor parameter validation with detailed error context
- Execution-time dependency checking (null pointer validation)
- Image index bounds checking for swapchain operations
- Debug logging with configurable intervals to reduce console spam

### Threading and Synchronization
- All nodes execute on single frame graph thread
- GPU queue synchronization handled by frame graph barriers
- Async compute support planned but not yet implemented
- Resource transitions managed automatically by frame graph

### Conventions
- Resource IDs use `FrameGraphTypes::ResourceId` type (uint32_t)
- All external dependencies passed as non-owning pointers
- Push constants must be 16-byte aligned for GPU compatibility
- Debug names included in resource creation for debugging

---

**Update Note**: This file references changeable components (pipeline managers, resource contexts, ECS systems). If any referenced interfaces or data structures change, this documentation must be updated to maintain accuracy for AI agents.