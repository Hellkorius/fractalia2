# Fractalia2 Engine Architecture

## Technology Stack
- **Graphics**: Vulkan 1.3 with frame graph
- **ECS**: Flecs entity component system
- **Platform**: SDL3
- **Math**: GLM
- **Build**: MinGW cross-compilation

## Modular Services

**RenderFrameDirector** (`src/vulkan/render_frame_director.*`)
- Frame execution: acquire → setup → compile → configure → execute → submit
- Node configuration with stored IDs and world reference
- `directFrame()` main execution method

**FrameGraph** (`src/vulkan/frame_graph.*`)
- Resource dependency management
- Compile-once execution model
- Automatic barrier insertion for compute→graphics transitions
- External resource import

**CommandSubmissionService** (`src/vulkan/command_submission_service.*`)
- GPU queue submission
- Multi-frame synchronization
- Fence/semaphore management

**FrameGraphResourceRegistry** (`src/vulkan/frame_graph_resource_registry.*`)
- Entity/position buffer registration and ID mapping
- Resource import coordination

**GPUSynchronizationService** (`src/vulkan/gpu_synchronization_service.*`)
- Per-frame fence management
- Compute/graphics synchronization

**PresentationSurface** (`src/vulkan/presentation_surface.*`)
- Swapchain lifecycle
- Surface acquisition and presentation

## Data Flow

```
CPU (Flecs ECS) → GPU Compute (Random Walk) → GPU Graphics (Instanced Rendering)
```

### Memory Layout
```cpp
struct GPUEntity {
    glm::vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // center.xyz, movementType  
    glm::vec4 runtimeState;       // totalTime, initialized, stateTimer, entityState
    glm::vec4 color;              // RGBA color
    glm::mat4 modelMatrix;        // transform matrix
}; // 128 bytes (2 cache lines)
```

## Node Architecture
- **EntityComputeNode**: Random walk movement, 600-frame interpolation cycles
- **EntityGraphicsNode**: Instanced rendering with camera matrix integration
- **SwapchainPresentNode**: Presentation with proper image indexing

## Fixes - January 2025

### Frame Graph Compilation Loop
- **Issue**: `reset()` clearing `compiled` flag every frame
- **Fix**: Preserve `compiled` state after first compilation
- **Files**: `src/vulkan/frame_graph.cpp:349`

### Frame Synchronization
- **Issue**: Frame counter stuck at 0
- **Fix**: Separate `frameCounter` (compute) vs `currentFrame` (buffer sync)
- **Files**: `src/vulkan/frame_graph.cpp:222`, `src/vulkan/render_frame_director.cpp:221`

### Camera-Viewport Integration
- **Issue**: World reference timing
- **Fix**: Configure nodes between acquisition and execution
- **Files**: `src/vulkan/render_frame_director.cpp:75`

### Node Configuration
- **Issue**: Hard-coded node IDs
- **Fix**: Store actual node IDs from `addNode()`
- **Files**: `src/vulkan/render_frame_director.h:86-88`, `src/vulkan/render_frame_director.cpp:147-174`

## Performance
- **Entities**: 80,000 at 60 FPS
- **Frame Time**: 16.67ms consistent
- **Workgroups**: 2,500 workgroups (32 threads each)
- **Memory**: Cache-optimized 128-byte entity layout
- **Movement**: 600-frame interpolated cycles with staggering

## Extension Points
- Multi-pass rendering via frame graph nodes
- Post-processing shader chains
- Shadow mapping with additional depth passes
- Async compute with multi-queue scheduling
- Resource streaming via external import system