# Fractalia2 Project Documentation

## Overview
Cross-compiled (Linux→Windows) engine for 80,000+ entities at 60 FPS using GPU-driven instanced rendering.

## Stack
- SDL3, Vulkan, Flecs ECS, GLM, MinGW

## Architecture

### CPU/GPU Pipeline
1. **CPU (Flecs ECS)**: Entity lifecycle, input, camera
2. **GPU Compute**: Random walk movement calculation  
3. **GPU Graphics**: Instanced rendering
4. **Bridge**: GPUEntityManager handles CPU→GPU transfer

### Data Flow
1. Flecs components → GPUEntity structs (128-byte layout)
2. Compute shader calculates positions with interpolation  
3. Graphics reads entity buffer + position buffer
4. Single instanced draw call

## Structure
```
src/
├── main.cpp                    # Application entry
├── vulkan_renderer.*           # Master renderer, frame loop
├── vulkan/                     # Vulkan subsystems
│   ├── frame_graph.*           # ✅ Render graph coordinator
│   ├── nodes/                  # ✅ Render graph nodes
│   │   ├── entity_compute_node.*    # ✅ Compute movement
│   │   ├── entity_graphics_node.*   # ✅ Graphics rendering
│   │   └── swapchain_present_node.* # ✅ Presentation
│   ├── vulkan_context.*        # Instance, device, queue
│   ├── vulkan_swapchain.*      # Swapchain, MSAA, framebuffers
│   ├── vulkan_pipeline.*       # Graphics/compute pipelines
│   ├── resource_context.*      # Buffer/memory manager
│   └── vulkan_sync.*           # Fences, semaphores
├── ecs/                        # ECS components and systems
│   ├── gpu_entity_manager.*    # CPU→GPU bridge
│   ├── systems/                # ECS systems
│   └── components/             # Component definitions
└── shaders/
    ├── movement_random.comp    # Compute movement
    ├── vertex.vert            # Vertex shader
    └── fragment.frag          # Fragment shader
```


## GPUEntity Structure
```cpp
struct GPUEntity {
    glm::mat4 modelMatrix;        // 64 bytes - world transform
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset  
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 color;              // 16 bytes - RGBA color
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, stateTimer, entityState
}; // 128 bytes total
```

## Movement System
Random walk algorithm with interpolated movement:
- 32 threads per workgroup compute dispatch
- 600-frame cycles with smooth interpolation
- Staggered computation to avoid frame spikes

## Modular Architecture

### Services
- **RenderFrameDirector**: Frame execution orchestration
- **CommandSubmissionService**: GPU queue submission management  
- **FrameGraphResourceRegistry**: Resource registration with frame graph
- **GPUSynchronizationService**: GPU synchronization primitives
- **PresentationSurface**: Swapchain and presentation management

## Architecture Fixes - January 2025

### Frame Graph Compilation Loop
- **Issue**: `FrameGraph::reset()` clearing `compiled` flag every frame
- **Fix**: Preserve `compiled` state in `reset()` method
- **File**: `src/vulkan/frame_graph.cpp:349`

### Frame Synchronization  
- **Issue**: `frameCounter` stuck at 0, preventing movement cycles
- **Fix**: Pass `frameCounter` to compute nodes, `currentFrame` to graphics nodes
- **Files**: `src/vulkan/frame_graph.cpp:222`, `src/vulkan/render_frame_director.cpp:221`

### Camera Viewport Integration
- **Issue**: World reference not reaching graphics nodes
- **Fix**: Call `configureFrameGraphNodes()` between acquisition and execution
- **File**: `src/vulkan/render_frame_director.cpp:75`

### Node Configuration Architecture
- **Issue**: Hard-coded node IDs causing configuration failures
- **Fix**: Store node IDs from `addNode()` return values, use for configuration
- **Files**: `src/vulkan/render_frame_director.h:86-88`, `src/vulkan/render_frame_director.cpp:147-174`

## Current Status
- **Entities**: 80,000 at 60 FPS
- **Movement**: 600-frame interpolated random walk cycles
- **Camera**: WASD + mouse wheel + rotation controls
- **Stability**: Zero crashes, single frame graph compilation
- **Features**: Entity spawning, compute-driven movement, camera integration