# Vulkan Services

## Overview
High-level Vulkan services that orchestrate frame rendering coordination, command submission, synchronization, and error recovery for the 80,000+ entity GPU-driven rendering pipeline.

## Architecture

### Service Coordination Pattern
Services follow a dependency injection pattern with clear separation of concerns:

```cpp
// Frame rendering orchestration
RenderFrameDirector -> CommandSubmissionService -> GPUSynchronizationService
                  -> PresentationSurface -> ErrorRecoveryService
                  -> FrameStateManager
```

### Data Flow
1. **Frame Direction**: RenderFrameDirector coordinates frame execution through FrameGraph
2. **Command Submission**: Async compute (frame N+1) + graphics (frame N) submission
3. **Synchronization**: Per-frame fence management with robust waiting
4. **Presentation**: Swapchain image acquisition and recreation handling
5. **Error Recovery**: Automatic swapchain recreation and frame retry logic

## Core Services

### RenderFrameDirector
**Purpose**: Main frame coordination orchestrator

**Key Responsibilities**:
- FrameGraph setup and node configuration
- Resource ID management (entity buffers, position buffers, swapchain images)
- Frame compilation and execution coordination
- Swapchain cache invalidation on recreation

**Critical Pattern**:
```cpp
RenderFrameResult directFrame(uint32_t currentFrame, float totalTime, float deltaTime, 
                             uint32_t frameCounter, flecs::world* world);
```

### CommandSubmissionService
**Purpose**: Async compute + graphics command submission

**Key Responsibilities**:
- Parallel compute/graphics work submission (compute frame N+1, graphics frame N)
- Queue management through QueueManager dependency
- Fence-based synchronization coordination
- Swapchain recreation detection

**Critical Pattern**:
```cpp
// ASYNC COMPUTE: Submit compute for next frame while graphics renders current
submitComputeWorkAsync(currentFrame + 1);  // No waiting
submitGraphicsWork(currentFrame);          // With sync
```

### GPUSynchronizationService
**Purpose**: Per-frame fence management with RAII safety

**Key Responsibilities**:
- Dual fence management (compute + graphics per frame)
- Robust fence waiting with timeout handling
- In-use tracking to prevent double-submission
- RAII cleanup before context destruction

**Critical Pattern**:
```cpp
std::array<vulkan_raii::Fence, MAX_FRAMES_IN_FLIGHT> computeFences;
std::array<vulkan_raii::Fence, MAX_FRAMES_IN_FLIGHT> graphicsFences;
```

### PresentationSurface
**Purpose**: Swapchain image acquisition and recreation coordination

**Key Responsibilities**:
- Next image acquisition with error handling
- Swapchain recreation state management
- Framebuffer resize detection and coordination
- Render pass tracking for recreation

**Critical Pattern**:
```cpp
SurfaceAcquisitionResult acquireNextImage(uint32_t currentFrame);
bool recreateSwapchain(); // Coordinates with graphics pipeline manager
```

### FrameStateManager
**Purpose**: Per-frame usage tracking for fence optimization

**Key Responsibilities**:
- Track compute/graphics usage per frame
- Provide fence lists for selective waiting
- Optimize fence waits (skip unused work)

**Critical Pattern**:
```cpp
struct FrameState {
    bool computeUsed = true;   // Initialize true for safety
    bool graphicsUsed = true;
};
```

### ErrorRecoveryService
**Purpose**: Automatic error recovery and retry logic

**Key Responsibilities**:
- Frame failure analysis and recovery decisions
- Swapchain recreation coordination
- Frame retry after successful recovery

**Critical Pattern**:
```cpp
bool handleFrameFailure(const RenderFrameResult& frameResult, /* retry params */);
```

## Service Integration Patterns

### Dependency Chain
```cpp
// Service initialization with clear dependencies
RenderFrameDirector.initialize(context, swapchain, pipelineSystem, sync, 
                              resourceCoordinator, gpuEntityManager, 
                              frameGraph, presentationSurface);
```

### Error Propagation
```cpp
struct SubmissionResult {
    bool success = false;
    bool swapchainRecreationNeeded = false;
    VkResult lastResult = VK_SUCCESS;
};
```

### Async Coordination
- **Compute Pipeline**: Frame N+1 position calculation (no sync dependencies)
- **Graphics Pipeline**: Frame N rendering (waits for compute N completion)
- **Presentation**: Image acquisition → graphics → present with error recovery

## Critical Implementation Details

### Fence Management
- **RAII Safety**: vulkan_raii::Fence prevents resource leaks
- **Robust Waiting**: Timeout handling with detailed error reporting
- **Usage Tracking**: Prevent fence double-submission and optimize waits

### Frame Synchronization
- **Global Frame Counter**: Atomic counter for compute shader consistency
- **Per-Frame Resources**: Independent resource sets for MAX_FRAMES_IN_FLIGHT
- **Cache Invalidation**: Swapchain recreation invalidates cached resource IDs

### Error Recovery Strategy
1. Detect frame failure (acquisition, submission, or presentation)
2. Analyze error type (swapchain out-of-date, device lost, etc.)
3. Attempt swapchain recreation if appropriate
4. Retry frame execution with fresh resources
5. Propagate unrecoverable errors to caller

## Performance Characteristics
- **Async Compute**: Overlapped compute/graphics execution for maximum GPU utilization
- **Fence Optimization**: Skip unnecessary fence waits based on usage tracking
- **Resource Caching**: Minimize resource allocation/deallocation overhead
- **Robust Recovery**: Automatic swapchain recreation without frame drops