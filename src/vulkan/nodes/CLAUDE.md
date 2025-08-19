# Vulkan Render Graph Nodes

## Purpose
Concrete FrameGraphNode implementations that execute the Fractalia2 rendering pipeline stages. Each node encapsulates a discrete GPU operation (compute, graphics, presentation) with explicit resource dependencies and queue requirements.

## Architecture
```
nodes/
├── CLAUDE.md                    # Technical documentation
├── changelog.md                 # Code quality improvements (2025-01-18)
├── entity_compute_node.{h,cpp}  # Chunked compute dispatch for entity movement
├── entity_graphics_node.{h,cpp} # Single instanced draw call with MSAA
└── swapchain_present_node.{h,cpp} # Presentation dependency declaration
```

## Input/Output Documentation

### EntityComputeNode (`entity_compute_node.{h,cpp}`)
**Purpose**: Chunked compute dispatch for entity movement calculations using random walk algorithm

**Constructor Parameters**:
- `entityBufferId`: GPUEntity buffer (128-byte structs)
- `positionBufferId`: Output position buffer 
- `currentPositionBufferId`: Current frame positions (ReadWrite)
- `targetPositionBufferId`: Target interpolation positions (ReadWrite)
- `ComputePipelineManager*`: Pipeline state management
- `GPUEntityManager*`: Entity count and descriptor sets
- `GPUTimeoutDetector*` (optional): Timeout detection

**Resource Dependencies**:
- **Inputs**: entityBuffer, currentPositionBuffer, targetPositionBuffer (ReadWrite, ComputeShader)
- **Outputs**: positionBuffer (Write, ComputeShader)
- **Queue**: Compute only (`needsComputeQueue() = true`)

**Execution Flow**:
1. Validates dependencies and entity count (early return if zero)
2. Creates compute pipeline from DescriptorLayoutPresets::createEntityComputeLayout()
3. Updates push constants with frame data (time, deltaTime, entityCount, frame, entityOffset)
4. Executes chunked dispatch: 64 threads/workgroup, max 512 workgroups per chunk
5. Uses adaptive dispatch parameters to prevent GPU timeouts

**Push Constants Structure**:
```cpp
struct ComputePushConstants {
    float time, deltaTime;
    uint32_t entityCount, frame, entityOffset;
    uint32_t padding[3]; // 16-byte alignment
};
```

**Key Methods**:
- `updateFrameData(float, float, uint32_t)`: Updates frame timing
- `executeChunkedDispatch()`: Internal chunking logic

### EntityGraphicsNode (`entity_graphics_node.{h,cpp}`)
**Purpose**: Single instanced draw call rendering with MSAA and uniform buffer caching

**Constructor Parameters**:
- `entityBufferId`: GPUEntity buffer (color/transform data)
- `positionBufferId`: Position buffer from compute node
- `colorTarget`: Static placeholder (unused, uses dynamic swapchain ID)
- `GraphicsPipelineManager*`: Pipeline state management
- `VulkanSwapchain*`: MSAA framebuffers and render pass
- `ResourceContext*`: Memory management
- `GPUEntityManager*`: Entity count and descriptor access

**Resource Dependencies**:
- **Inputs**: entityBuffer, positionBuffer (Read, VertexShader)
- **Outputs**: currentSwapchainImageId (Write, ColorAttachment) - resolved dynamically
- **Queue**: Graphics only (`needsGraphicsQueue() = true`)

**Execution Flow**:
1. Validates dependencies and entity count (early return if zero)
2. Updates uniform buffer with camera matrices (dirty tracking optimization)
3. Creates graphics pipeline from DescriptorLayoutPresets::createEntityGraphicsLayout()
4. Begins render pass with MSAA (VK_SAMPLE_COUNT_2_BIT)
5. Binds pipeline, descriptor sets, and executes single instanced draw
6. Ends render pass with automatic MSAA resolve

**Uniform Buffer Structure**:
```cpp
struct CachedUBO {
    glm::mat4 view;
    glm::mat4 proj;
};
```

**Key Methods**:
- `setImageIndex(uint32_t)`: Current swapchain image
- `setCurrentSwapchainImageId(ResourceId)`: Dynamic resource resolution
- `updateFrameData(float, float, uint32_t)`: Frame timing
- `setWorld(flecs::world*)`: ECS world for camera access
- `markUniformBufferDirty()`: Force uniform update
- `getCameraMatrices()`, `updateUniformBufferData()`: Internal helpers

### SwapchainPresentNode (`swapchain_present_node.{h,cpp}`)
**Purpose**: Presentation dependency declaration - no actual command buffer operations

**Constructor Parameters**:
- `colorTarget`: Static placeholder (unused, uses dynamic swapchain ID)
- `VulkanSwapchain*`: Swapchain validation and bounds checking

**Resource Dependencies**:
- **Inputs**: currentSwapchainImageId (Read, ColorAttachment) - resolved dynamically
- **Outputs**: None (presents to OS window system)
- **Queue**: Graphics only (`needsGraphicsQueue() = true`)

**Execution Flow**:
1. Validates swapchain dependency and image index bounds
2. **No command buffer operations** - presentation handled at queue submission level
3. Primary function is dependency declaration for proper frame graph synchronization

**Key Methods**:
- `setImageIndex(uint32_t)`: Current swapchain image index
- `setCurrentSwapchainImageId(ResourceId)`: Dynamic resource resolution  
- `getImageIndex() const`: Image index for debugging/validation

## System Integration

### Core Vulkan Infrastructure
- `../core/vulkan_context.*`: Device, queue family management, VulkanFunctionLoader access
- `../core/vulkan_swapchain.*`: Framebuffers, render pass, MSAA configuration
- `../core/queue_manager.*`: Queue allocation and submission coordination

### Pipeline Management
- `../pipelines/compute_pipeline_manager.*`: Compute pipeline caching and dispatch execution
- `../pipelines/graphics_pipeline_manager.*`: Graphics pipeline states, render pass creation
- `../pipelines/descriptor_layout_manager.*`: Layout presets (EntityComputeLayout, EntityGraphicsLayout)

### Resource Management  
- `../resources/resource_context.*`: Buffer allocation, memory management
- `../resources/command_executor.*`: Command buffer execution coordination
- `../rendering/frame_graph.*`: Resource dependency tracking, barrier insertion

### ECS Integration
- `../../ecs/gpu_entity_manager.*`: Entity count, descriptor sets, CPU-GPU data transfer
- `../../ecs/systems/camera_system.*`: Camera matrix computation from flecs world
- `../../ecs/camera_component.h`: Camera component definitions

### Performance Monitoring
- `../monitoring/gpu_timeout_detector.*`: Optional compute timeout detection

## Technical Implementation Notes

### Resource Management
- **Dynamic Resource Resolution**: Both graphics and present nodes use `currentSwapchainImageId` resolved per-frame
- **Static vs Dynamic IDs**: Constructor colorTarget parameters are static placeholders, actual resources set via `setCurrentSwapchainImageId()`
- **Queue Requirements**: Compute node requires compute queue, graphics/present nodes require graphics queue

### Performance Optimization
- **Chunked Compute Dispatch**: 64 threads/workgroup, max 512 workgroups/chunk to prevent GPU timeouts
- **Uniform Buffer Caching**: Graphics node uses dirty tracking to avoid redundant camera matrix updates
- **Debug Counter Throttling**: Atomic counters with modulo logging to reduce console spam

### Error Handling
- **Constructor Validation**: Fail-fast null pointer checks during construction
- **Runtime Validation**: Dependency verification during execute() with early returns
- **Bounds Checking**: Swapchain image index validation in present node

### Data Structures
- **ComputePushConstants**: 16-byte aligned with padding for GPU compatibility
- **CachedUBO**: View/projection matrices with dirty tracking optimization
- **GPUEntity**: 128-byte cache-optimized layout (referenced, not defined here)

### Frame Graph Integration
- **Resource Dependencies**: Explicit input/output declarations for automatic barrier insertion
- **Queue Coordination**: Queue requirements declared via `needsComputeQueue()`/`needsGraphicsQueue()`
- **Dynamic Dependencies**: Swapchain image dependencies resolved at runtime

---
*Last Updated: 2025-01-18 - Reflects current implementation after code quality improvements*