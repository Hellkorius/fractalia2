# Vulkan Services - High-Level Frame Orchestration & GPU Management

## Purpose
Provides high-level orchestration services for frame execution, GPU synchronization, command submission, and presentation management. Acts as the coordination layer between the frame graph execution and low-level Vulkan infrastructure.

## File/Folder Hierarchy
```
vulkan/services/
├── CLAUDE.md                           # This documentation
├── render_frame_director.*             # Master frame orchestrator & frame graph coordinator
├── command_submission_service.*        # GPU queue submission & async compute management
├── gpu_synchronization_service.*       # Fence management & GPU timeline synchronization
└── presentation_surface.*              # Swapchain acquisition & recreation coordination
```

## Inputs & Outputs (by Service)

### RenderFrameDirector
**Primary Purpose**: Master orchestrator for entire frame execution pipeline

**Inputs**:
- `VulkanContext*` - Core Vulkan infrastructure (device, queues, function loader)
- `VulkanSwapchain*` - Swapchain for image acquisition and extent information
- `PipelineSystemManager*` - Pipeline management for compute/graphics operations
- `VulkanSync*` - Synchronization objects and command buffers
- `ResourceContext*` - Memory allocation and resource management
- `GPUEntityManager*` - CPU↔GPU entity data bridge
- `MovementCommandProcessor*` - Movement system integration
- `FrameGraph*` - Render graph execution engine
- **Per-frame data**: currentFrame, totalTime, deltaTime, frameCounter, flecs::world*

**Outputs**:
- `RenderFrameResult` - Frame execution success status + swapchain image index
- `FrameGraph::ExecutionResult` - Command buffer usage flags and execution metadata
- **Side effects**: Frame graph node configuration, resource ID updates, swapchain image acquisition

**Key Data Structures**:
```cpp
struct RenderFrameResult {
    bool success;                           // Frame execution success
    uint32_t imageIndex;                   // Acquired swapchain image index
    FrameGraph::ExecutionResult executionResult; // Command buffer usage and metadata
};
```

**API Surface**:
- `initialize(context, swapchain, pipelineSystem, sync, resourceContext, gpuEntityManager, movementCommandProcessor, frameGraph)` → `bool`
- `directFrame(currentFrame, totalTime, deltaTime, frameCounter, world)` → `RenderFrameResult`
- `updateResourceIds(entityBufferId, positionBufferId, currentPositionBufferId, targetPositionBufferId)` → `void`
- `configureFrameGraphNodes(imageIndex, world)` → `void`

**Internal State Management**:
- Frame graph node IDs: computeNodeId, graphicsNodeId, presentNodeId
- Resource IDs: entityBufferId, positionBufferId, currentPositionBufferId, targetPositionBufferId, swapchainImageId
- Cached swapchain image IDs per frame
- Frame graph initialization tracking

### CommandSubmissionService
**Primary Purpose**: GPU queue submission and async compute coordination

**Inputs**:
- `VulkanContext*` - Device and queue access
- `VulkanSync*` - Synchronization objects for submission
- `VulkanSwapchain*` - Swapchain for presentation coordination
- `FrameGraph::ExecutionResult` - Command buffers and usage flags from frame execution
- **Per-submission data**: currentFrame, imageIndex, framebufferResized flag

**Outputs**:
- `SubmissionResult` - Submission success, swapchain recreation needs, Vulkan result codes
- **Side effects**: GPU queue submissions (compute + graphics), presentation queue operations

**Key Data Structures**:
```cpp
struct SubmissionResult {
    bool success;                      // Overall submission success
    bool swapchainRecreationNeeded;   // Indicates need for swapchain recreation
    VkResult lastResult;              // Last Vulkan operation result
};
```

**API Surface**:
- `initialize(context, sync, swapchain)` → `bool`
- `submitFrame(currentFrame, imageIndex, executionResult, framebufferResized)` → `SubmissionResult`

**Async Compute Pipeline**:
- Submits compute work for frame N+1 while graphics renders frame N
- Parallel compute/graphics execution with proper synchronization
- Fence-based timeline management for overlapped execution

### GPUSynchronizationService
**Primary Purpose**: Fence management and GPU timeline synchronization

**Inputs**:
- `VulkanContext&` - Device context for fence creation
- `MAX_FRAMES_IN_FLIGHT` constant - Frame overlap configuration
- **Per-operation data**: Frame indices for fence access

**Outputs**:
- `VkFence` handles for compute and graphics operations per frame
- **Per-frame state**: Fence usage tracking (in-use flags)
- `VkResult` codes for fence wait operations

**API Surface**:
- `initialize(context)` → `bool`
- `waitForComputeFence(frameIndex, fenceName)` → `VkResult`
- `waitForGraphicsFence(frameIndex, fenceName)` → `VkResult`
- `getComputeFence(frameIndex)` → `VkFence`
- `getGraphicsFence(frameIndex)` → `VkFence`
- `isComputeInUse(frameIndex)` / `setComputeInUse(frameIndex, inUse)` → `bool` / `void`
- `isGraphicsInUse(frameIndex)` / `setGraphicsInUse(frameIndex, inUse)` → `bool` / `void`
- `waitForAllFrames()` → `bool`

**Synchronization Objects**:
- Per-frame compute fences (MAX_FRAMES_IN_FLIGHT array)
- Per-frame graphics fences (MAX_FRAMES_IN_FLIGHT array)
- In-use tracking for proper fence lifecycle management

### PresentationSurface
**Primary Purpose**: Swapchain image acquisition and recreation coordination

**Inputs**:
- `VulkanContext*` - Device and function loader access
- `VulkanSwapchain*` - Swapchain management
- `PipelineSystemManager*` - Pipeline invalidation during recreation
- `GPUSynchronizationService*` - Fence synchronization during recreation
- **Per-frame data**: currentFrame for acquisition timing

**Outputs**:
- `SurfaceAcquisitionResult` - Image acquisition success, image index, recreation flags
- **Side effects**: Swapchain recreation, pipeline cache invalidation, fence synchronization

**Key Data Structures**:
```cpp
struct SurfaceAcquisitionResult {
    bool success;               // Acquisition success status
    uint32_t imageIndex;       // Acquired swapchain image index
    bool recreationNeeded;     // Indicates need for swapchain recreation
    VkResult result;           // Vulkan acquisition result
};
```

**API Surface**:
- `initialize(context, swapchain, pipelineSystem, syncManager)` → `bool`
- `acquireNextImage(currentFrame)` → `SurfaceAcquisitionResult`
- `recreateSwapchain()` → `bool`
- `setFramebufferResized(resized)` / `isFramebufferResized()` → `void` / `bool`

**Recreation Coordination**:
- Detects window resize events and VK_ERROR_OUT_OF_DATE_KHR
- Coordinates with pipeline system for cache invalidation
- Manages recreation-in-progress state to prevent concurrent recreation

## Data Flow Between Services

### Frame Execution Sequence
1. **RenderFrameDirector::directFrame()** - Master coordination entry point
2. **PresentationSurface::acquireNextImage()** - Swapchain image acquisition
3. **RenderFrameDirector** configures frame graph nodes with acquired image
4. **FrameGraph::execute()** - Generate command buffers (compute + graphics)
5. **CommandSubmissionService::submitFrame()** - Submit command buffers to GPU queues
6. **CommandSubmissionService** internal presentation - Present rendered frame

### Synchronization Flow
1. **GPUSynchronizationService** provides per-frame fences
2. **CommandSubmissionService** uses fences for queue submissions
3. **RenderFrameDirector** waits on appropriate fences before frame graph execution
4. **PresentationSurface** coordinates with sync service during swapchain recreation

### Error Recovery Flow
1. **PresentationSurface** detects VK_ERROR_OUT_OF_DATE_KHR or window resize
2. **PresentationSurface::recreateSwapchain()** coordinates full recreation
3. **GPUSynchronizationService::waitForAllFrames()** ensures GPU idle
4. **PipelineSystemManager** invalidates affected pipelines
5. **RenderFrameDirector** reinitializes frame graph with new swapchain

## Peripheral Dependencies

### Core Infrastructure (Services consume):
- **vulkan/core/vulkan_context.h** - Device, queue, and function loader access
- **vulkan/core/vulkan_swapchain.h** - Swapchain management and image access
- **vulkan/core/vulkan_sync.h** - Command pools and basic synchronization
- **vulkan/core/vulkan_constants.h** - MAX_FRAMES_IN_FLIGHT and timeout configuration

### Pipeline & Resource Systems:
- **vulkan/pipelines/pipeline_system_manager.h** - Unified pipeline management
- **vulkan/resources/resource_context.h** - Memory allocation and buffer management
- **vulkan/rendering/frame_graph.h** - Render graph execution engine

### ECS Integration:
- **ecs/gpu_entity_manager.h** - CPU↔GPU entity data synchronization
- **ecs/movement_command_system.h** - Movement command processing
- **flecs.h** - ECS world access for component data

### External Consumers (Services used by):
- **vulkan_renderer.cpp** - Main renderer owns and orchestrates all services
- **vulkan/nodes/entity_compute_node.h** - Uses frame graph integration points
- **vulkan/nodes/entity_graphics_node.h** - Uses frame graph integration points
- **vulkan/nodes/swapchain_present_node.h** - Uses presentation coordination

## Key Integration Points

### Frame Graph Coordination
RenderFrameDirector ↔ FrameGraph: Node configuration, resource binding, execution control
FrameGraph ↔ CommandSubmissionService: Command buffer handoff for GPU submission

### Async Compute Pipeline
CommandSubmissionService implements N+1 frame async compute:
- Frame N: Graphics renders, Compute calculates frame N+1 movement
- Overlapped execution maximizes GPU utilization
- Proper fence synchronization prevents data races

### Window System Integration
PresentationSurface ↔ SDL Window Events: Framebuffer resize detection
SwapchainRecreation → Pipeline invalidation → Resource reallocation cascade

### Resource Lifecycle
RenderFrameDirector manages frame graph resource IDs:
- Entity buffers (static) + Position buffers (dynamic per frame)
- Swapchain images (per presentation target)
- Resource binding updates for compute output → graphics input

## Key Notes

### Performance Considerations
- **Async Compute**: Overlapped compute/graphics execution for maximum GPU utilization
- **Fence Management**: Minimal fence waits, proper in-use tracking prevents stalls
- **Command Submission**: Batched submissions reduce driver overhead
- **Recreation Efficiency**: Minimal work during swapchain recreation (fence waits only)

### Error Handling Robustness
- **Graceful Degradation**: Services return detailed error status for upstream handling
- **Recreation Recovery**: Automatic swapchain recreation with full pipeline coordination
- **Fence Timeouts**: Robust fence waiting with timeout handling and debug output

### Thread Safety
- **Single-threaded**: All services designed for main thread execution only
- **State Consistency**: Services maintain consistent state across frame boundaries
- **Fence Synchronization**: GPU timeline properly synchronized with CPU frame progression

### Critical Dependencies
- **Initialization Order**: VulkanContext → GPUSynchronizationService → other services
- **Cleanup Order**: Services → Core infrastructure (reverse initialization order)
- **Frame Graph Integration**: RenderFrameDirector must be configured before frame execution

## Update Requirements
**IMPORTANT**: This documentation must be updated if any referenced components change:
- Frame graph execution model changes (affects RenderFrameDirector coordination)
- Vulkan synchronization model changes (affects GPUSynchronizationService fence management)
- Swapchain recreation procedures change (affects PresentationSurface error handling)
- Async compute pipeline changes (affects CommandSubmissionService submission ordering)
- Resource management patterns change (affects RenderFrameDirector resource ID tracking)
- ECS integration points change (affects frame data inputs and GPU entity coordination)