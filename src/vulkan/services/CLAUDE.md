# Vulkan Services - High-Level Frame Orchestration & GPU Management

## Purpose
Provides high-level orchestration services for frame execution, GPU synchronization, command submission, presentation management, error recovery, and frame state tracking. Acts as the coordination layer between the frame graph execution and low-level Vulkan infrastructure, integrating with QueueManager for optimal command buffer and queue management.

## File/Folder Hierarchy
```
vulkan/services/
├── CLAUDE.md                           # This documentation
├── render_frame_director.*             # Master frame orchestrator & frame graph coordinator
├── command_submission_service.*        # GPU queue submission & async compute management
├── gpu_synchronization_service.*       # Fence management & GPU timeline synchronization
├── presentation_surface.*              # Swapchain acquisition & recreation coordination
├── error_recovery_service.*            # Frame failure recovery & automatic swapchain recreation
└── frame_state_manager.*               # Frame state tracking for optimal fence management
```

## Inputs & Outputs (by Service)

### RenderFrameDirector
**Primary Purpose**: Master orchestrator for entire frame execution pipeline

**Inputs**:
- `VulkanContext*` - Core Vulkan infrastructure (device, function loader)
- `VulkanSwapchain*` - Swapchain for image acquisition and extent information
- `PipelineSystemManager*` - Pipeline management for compute/graphics operations
- `VulkanSync*` - Synchronization objects (semaphores/fences)
- `ResourceContext*` - Memory allocation and resource management
- `GPUEntityManager*` - CPU↔GPU entity data bridge
- `FrameGraph*` - Render graph execution engine
- `PresentationSurface*` - Swapchain image acquisition coordination
- **Per-frame data**: currentFrame, totalTime, deltaTime, frameCounter, flecs::world*

**Outputs**:
- `RenderFrameResult` - Frame execution success status + swapchain image index + execution metadata
- **Side effects**: Frame graph node configuration, resource ID updates, movement command processing

**Key Data Structures**:
```cpp
struct RenderFrameResult {
    bool success = false;                   // Frame execution success
    uint32_t imageIndex = 0;               // Acquired swapchain image index
    FrameGraph::ExecutionResult executionResult; // Command buffer usage and metadata
};
```

**API Surface**:
- `initialize(context, swapchain, pipelineSystem, sync, resourceContext, gpuEntityManager, frameGraph, presentationSurface)` → `bool`
- `directFrame(currentFrame, totalTime, deltaTime, frameCounter, world)` → `RenderFrameResult`
- `updateResourceIds(entityBufferId, positionBufferId, currentPositionBufferId, targetPositionBufferId)` → `void`
- `configureFrameGraphNodes(imageIndex, world)` → `void`
- `resetSwapchainCache()` → `void`

**Internal State Management**:
- Frame graph node IDs: computeNodeId, graphicsNodeId, presentNodeId (using FrameGraphTypes::NodeId)
- Resource IDs: entityBufferId, positionBufferId, currentPositionBufferId, targetPositionBufferId, swapchainImageId (using FrameGraphTypes::ResourceId)
- Cached swapchain image IDs per frame
- Frame graph initialization tracking

### CommandSubmissionService
**Primary Purpose**: GPU queue submission and async compute coordination using QueueManager

**Inputs**:
- `VulkanContext*` - Device and function loader access
- `VulkanSync*` - Synchronization objects for submission (semaphores/fences)
- `VulkanSwapchain*` - Swapchain for presentation coordination
- `QueueManager*` - Centralized queue and command buffer management
- `FrameGraph::ExecutionResult` - Command buffer usage flags from frame execution
- **Per-submission data**: currentFrame, imageIndex, framebufferResized flag

**Outputs**:
- `SubmissionResult` - Submission success, swapchain recreation needs, Vulkan result codes
- **Side effects**: GPU queue submissions (compute + graphics), presentation queue operations, telemetry recording

**Key Data Structures**:
```cpp
struct SubmissionResult {
    bool success = false;              // Overall submission success
    bool swapchainRecreationNeeded = false; // Indicates need for swapchain recreation
    VkResult lastResult = VK_SUCCESS;  // Last Vulkan operation result
};
```

**API Surface**:
- `initialize(context, sync, swapchain, queueManager)` → `bool`
- `submitFrame(currentFrame, imageIndex, executionResult, framebufferResized)` → `SubmissionResult`

**Async Compute Pipeline**:
- Submits compute work for frame N+1 while graphics renders frame N
- Parallel compute/graphics execution with no synchronization dependencies
- Uses QueueManager for optimal command buffer selection and queue access
- Records queue utilization telemetry for performance monitoring

### GPUSynchronizationService
**Primary Purpose**: Fence management and GPU timeline synchronization using RAII wrappers

**Inputs**:
- `const VulkanContext&` - Device context for fence creation
- `MAX_FRAMES_IN_FLIGHT` constant - Frame overlap configuration
- **Per-operation data**: Frame indices for fence access

**Outputs**:
- `VkFence` handles for compute and graphics operations per frame (via RAII wrappers)
- **Per-frame state**: Fence usage tracking (in-use flags)
- `VkResult` codes for fence wait operations with robust timeout handling

**API Surface**:
- `initialize(const VulkanContext& context)` → `bool`
- `waitForComputeFence(frameIndex, fenceName = "compute")` → `VkResult`
- `waitForGraphicsFence(frameIndex, fenceName = "graphics")` → `VkResult`
- `getComputeFence(frameIndex)` → `VkFence`
- `getGraphicsFence(frameIndex)` → `VkFence`
- `isComputeInUse(frameIndex)` / `setComputeInUse(frameIndex, inUse)` → `bool` / `void`
- `isGraphicsInUse(frameIndex)` / `setGraphicsInUse(frameIndex, inUse)` → `bool` / `void`
- `waitForAllFrames()` → `bool`

**Synchronization Objects**:
- Per-frame compute fences using `vulkan_raii::Fence` (MAX_FRAMES_IN_FLIGHT array)
- Per-frame graphics fences using `vulkan_raii::Fence` (MAX_FRAMES_IN_FLIGHT array)
- In-use tracking for proper fence lifecycle management
- Robust timeout handling (2 seconds) with proper error propagation

### PresentationSurface
**Primary Purpose**: Swapchain image acquisition and recreation coordination with concurrency protection

**Inputs**:
- `VulkanContext*` - Device and function loader access
- `VulkanSwapchain*` - Swapchain management
- `GraphicsPipelineManager*` - Pipeline cache and render pass recreation
- `GPUSynchronizationService*` - Fence synchronization coordination
- **Per-frame data**: currentFrame for acquisition timing

**Outputs**:
- `SurfaceAcquisitionResult` - Image acquisition success, image index, recreation flags
- **Side effects**: Swapchain recreation, pipeline cache recreation, render pass recreation

**Key Data Structures**:
```cpp
struct SurfaceAcquisitionResult {
    bool success = false;       // Acquisition success status
    uint32_t imageIndex = 0;   // Acquired swapchain image index
    bool recreationNeeded = false; // Indicates need for swapchain recreation
    VkResult result = VK_SUCCESS; // Vulkan acquisition result
};
```

**API Surface**:
- `initialize(context, swapchain, graphicsManager, syncManager)` → `bool`
- `acquireNextImage(currentFrame)` → `SurfaceAcquisitionResult`
- `recreateSwapchain()` → `bool`
- `setFramebufferResized(resized)` / `isFramebufferResized()` → `void` / `bool`

**Recreation Coordination**:
- Detects window resize events and VK_ERROR_OUT_OF_DATE_KHR with timeout handling
- Coordinates render pass recreation followed by pipeline cache recreation
- Manages recreation-in-progress and acquisition-in-progress states for concurrency protection
- Critical fix: Pipeline cache recreation prevents corruption-related crashes

### ErrorRecoveryService
**Primary Purpose**: Frame failure recovery and automatic swapchain recreation handling

**Inputs**:
- `PresentationSurface*` - Swapchain recreation coordination
- `const RenderFrameResult&` - Failed frame result for analysis
- `RenderFrameDirector*` - Frame director for retry attempts
- **Frame retry data**: currentFrame, totalTime, deltaTime, frameCounter, flecs::world*

**Outputs**:
- `bool` - Recovery success status
- `RenderFrameResult&` - Retry frame result (out parameter)
- **Side effects**: Swapchain recreation, frame retry execution

**API Surface**:
- `initialize(presentationSurface)` → `void`
- `cleanup()` → `void`
- `handleFrameFailure(frameResult, frameDirector, currentFrame, totalTime, deltaTime, frameCounter, world, retryResult)` → `bool`

**Recovery Logic**:
- Analyzes frame failure conditions (framebuffer resize, acquisition timeouts)
- Determines if swapchain recreation is appropriate
- Coordinates with PresentationSurface for recreation
- Retries frame execution after successful recreation
- Provides detailed logging for debugging recovery scenarios

### FrameStateManager
**Primary Purpose**: Frame state tracking for optimal fence management and resource allocation

**Inputs**:
- `MAX_FRAMES_IN_FLIGHT` constant - Frame overlap configuration
- **Per-frame data**: frameIndex, computeUsed, graphicsUsed flags
- `VulkanSync*` - Synchronization service for fence access

**Outputs**:
- `std::vector<VkFence>` - Fences that need waiting for given frame
- `bool` - Active fence status for frame
- **State tracking**: Per-frame compute/graphics usage history

**Key Data Structures**:
```cpp
struct FrameState {
    bool computeUsed = true;  // Initialize to true for safety on first frame
    bool graphicsUsed = true;
};
```

**API Surface**:
- `initialize()` → `void`
- `cleanup()` → `void`
- `updateFrameState(frameIndex, computeUsed, graphicsUsed)` → `void`
- `getFencesToWait(frameIndex, sync)` → `std::vector<VkFence>`
- `hasActiveFences(frameIndex)` → `bool`

**Optimization Strategy**:
- Tracks actual usage patterns to avoid unnecessary fence waits
- Provides circular buffer logic for previous frame state access
- Initializes conservatively (all true) for first frame safety
- Enables selective fence waiting based on actual GPU work submission

## Data Flow Between Services

### Frame Execution Sequence
1. **RenderFrameDirector::directFrame()** - Master coordination entry point
2. **PresentationSurface::acquireNextImage()** - Swapchain image acquisition with timeout
3. **RenderFrameDirector** configures frame graph nodes with acquired image and world reference
4. **FrameGraph::execute()** - Generate command buffers using QueueManager command pools
5. **CommandSubmissionService::submitFrame()** - Submit command buffers via QueueManager queues
6. **CommandSubmissionService** internal presentation - Present rendered frame

### Error Recovery Sequence
1. **RenderFrameDirector::directFrame()** returns failure in `RenderFrameResult`
2. **ErrorRecoveryService::handleFrameFailure()** analyzes failure conditions
3. **ErrorRecoveryService** determines if swapchain recreation is appropriate
4. **PresentationSurface::recreateSwapchain()** coordinates full recreation
5. **ErrorRecoveryService** retries frame execution with new swapchain
6. **Frame retry success/failure** returned to caller for further handling

### Async Compute Flow
1. **CommandSubmissionService** submits compute for frame N+1 asynchronously
2. **CommandSubmissionService** submits graphics for frame N in parallel
3. **No synchronization** between compute and graphics (different buffer sets)
4. **QueueManager telemetry** records submission metrics for performance monitoring
5. **FrameStateManager** tracks actual usage patterns for optimization

### Synchronization Flow
1. **GPUSynchronizationService** provides RAII-managed fences per frame
2. **FrameStateManager** determines which fences need waiting based on usage history
3. **CommandSubmissionService** resets and uses fences for queue submissions
4. **VulkanSync** provides semaphores for swapchain/graphics coordination
5. **PresentationSurface** coordinates fence synchronization during recreation

### Resource Management Flow
1. **RenderFrameDirector** manages FrameGraphTypes::ResourceId mappings
2. **Frame graph** imports external resources (entity buffers, position buffers, swapchain images)
3. **CommandSubmissionService** uses command buffers from QueueManager pools
4. **GPUSynchronizationService** manages fence lifecycle via RAII wrappers
5. **PresentationSurface** triggers descriptor set updates during swapchain recreation

## Peripheral Dependencies

### Core Infrastructure (Services consume):
- **vulkan/core/vulkan_context.h** - Device and function loader access
- **vulkan/core/vulkan_swapchain.h** - Swapchain management and image access
- **vulkan/core/vulkan_sync.h** - Semaphores and basic synchronization
- **vulkan/core/queue_manager.h** - Centralized queue and command buffer management
- **vulkan/core/vulkan_constants.h** - MAX_FRAMES_IN_FLIGHT and timeout configuration
- **vulkan/core/vulkan_raii.h** - RAII wrappers for automatic Vulkan resource cleanup

### Pipeline & Resource Systems:
- **vulkan/pipelines/pipeline_system_manager.h** - Pipeline cache and render pass management
- **vulkan/pipelines/graphics_pipeline_manager.h** - Graphics pipeline recreation coordination
- **vulkan/resources/resource_context.h** - Memory allocation and descriptor set management
- **vulkan/rendering/frame_graph.h** - Render graph execution engine with resource management

### ECS Integration:
- **ecs/gpu_entity_manager.h** - CPU↔GPU entity data synchronization and compute descriptor sets
- **flecs.h** - ECS world access for component data

### External Consumers (Services used by):
- **vulkan_renderer.cpp** - Main renderer owns and orchestrates all services
- **vulkan/nodes/entity_compute_node.h** - Uses QueueManager compute command buffers
- **vulkan/nodes/entity_graphics_node.h** - Uses QueueManager graphics command buffers
- **vulkan/nodes/swapchain_present_node.h** - Uses presentation coordination

## Key Integration Points

### Frame Graph Coordination
RenderFrameDirector ↔ FrameGraph: Node configuration with dynamic swapchain image resolution, resource binding using FrameGraphTypes::ResourceId
FrameGraph ↔ QueueManager: Command buffer acquisition from specialized pools (Graphics: persistent+reset, Compute: transient)

### Async Compute Pipeline
CommandSubmissionService implements N+1 frame async compute via QueueManager:
- Frame N: Graphics renders, Compute calculates frame N+1 movement
- No synchronization dependencies - compute and graphics use separate buffer sets
- QueueManager provides optimal queue selection and telemetry recording
- FrameStateManager optimizes fence waiting based on actual usage patterns

### Error Recovery Integration
ErrorRecoveryService ↔ PresentationSurface: Automatic swapchain recreation on frame failures
ErrorRecoveryService ↔ RenderFrameDirector: Frame retry coordination with proper context
PresentationSurface ↔ GraphicsPipelineManager: Render pass and pipeline cache recreation

### RAII Resource Management
GPUSynchronizationService ↔ vulkan_raii: Automatic fence cleanup with explicit cleanup ordering
FrameGraph ↔ vulkan_raii: Automatic buffer/image cleanup for internal resources
All services use RAII patterns for exception safety and deterministic cleanup

### Window System Integration
PresentationSurface ↔ SDL Window Events: Framebuffer resize detection with concurrency protection
SwapchainRecreation → RenderPass recreation → Pipeline cache recreation → Descriptor set updates
ErrorRecoveryService coordinates with window events for proactive recreation

### Frame State Optimization
FrameStateManager ↔ CommandSubmissionService: Actual usage tracking for fence optimization
FrameStateManager ↔ GPUSynchronizationService: Selective fence waiting based on usage history
FrameStateManager provides circular buffer logic for previous frame state access

## Key Notes

### Performance Considerations
- **Async Compute**: True parallel compute/graphics with no synchronization overhead
- **QueueManager Integration**: Optimal command buffer allocation and queue selection
- **RAII Management**: Zero-overhead resource cleanup with proper destruction ordering
- **Telemetry Integration**: Queue utilization tracking for performance monitoring
- **Recreation Efficiency**: Minimal work during recreation with concurrency protection
- **Fence Optimization**: FrameStateManager reduces unnecessary fence waits

### Error Handling Robustness
- **Graceful Degradation**: Detailed error codes and status propagation
- **Timeout Handling**: 2-second timeouts for acquisition and fence waits
- **Corruption Prevention**: Pipeline cache recreation prevents second-resize crashes
- **Concurrency Protection**: In-progress flags prevent concurrent operations
- **Automatic Recovery**: ErrorRecoveryService handles common failure scenarios
- **Frame Retry Logic**: Intelligent retry attempts after successful recreation

### Memory Management
- **RAII Wrappers**: vulkan_raii::Fence and other wrappers ensure automatic cleanup
- **External Resources**: Frame graph imports but doesn't own swapchain/entity resources
- **Descriptor Set Updates**: Critical recreation handling for graphics and compute sets
- **State Tracking**: FrameStateManager provides minimal overhead usage tracking

### Critical Dependencies
- **Initialization Order**: VulkanContext → QueueManager → GPUSynchronizationService → other services
- **Cleanup Order**: cleanupBeforeContextDestruction() for RAII wrappers before context destruction
- **Frame Graph Integration**: Dynamic resource resolution requires proper node configuration
- **Error Recovery Chain**: ErrorRecoveryService → PresentationSurface → RenderFrameDirector coordination

## Update Requirements
**IMPORTANT**: This documentation must be updated if any referenced components change:
- Frame graph execution model changes (affects RenderFrameDirector coordination and resource ID management)
- QueueManager interface changes (affects CommandSubmissionService queue access and telemetry)
- RAII wrapper implementations change (affects GPUSynchronizationService fence management)
- Swapchain recreation procedures change (affects PresentationSurface coordination and pipeline cache handling)
- Async compute pipeline changes (affects CommandSubmissionService N+1 frame submission model)
- Resource management patterns change (affects RenderFrameDirector external resource imports)
- ECS integration points change (affects movement processing and entity data synchronization)
- Error recovery logic changes (affects ErrorRecoveryService failure analysis and retry coordination)
- Frame state tracking changes (affects FrameStateManager usage optimization and fence selection)