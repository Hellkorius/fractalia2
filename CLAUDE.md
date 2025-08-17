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

## AAA Modular Architecture ✅ OPERATIONAL

### Current State
- ✅ **Fully Modularized**: VulkanRenderer decomposed into 5 specialized services
- ✅ **Industry Names**: Proper AAA naming conventions throughout
- ✅ **Single Responsibility**: Each module has one clear purpose
- ✅ **Working**: 80,000 entities rendering with compute-driven movement

### Modules
**RenderFrameDirector**: Frame execution orchestration (`directFrame()`)
**CommandSubmissionService**: GPU queue submission management
**FrameGraphResourceRegistry**: Resource registration with frame graph
**GPUSynchronizationService**: GPU synchronization primitives
**PresentationSurface**: Swapchain and presentation management

## Recent Debugging Session - January 2025

### Issues Identified & Fixed
1. **Compute Shader Access** ✅ FIXED
   - Missing compute descriptor sets prevented buffer access
   - GPUEntityManager now creates and binds compute descriptor sets
   - EntityComputeNode properly binds descriptor sets before dispatch

2. **Entity Initialization** ✅ FIXED  
   - CPU was setting `initialized = 1.0f`, but compute shader expected `0.0f`
   - Fixed in `GPUEntity::fromECS()` - entities now start uninitialized
   - Compute shader properly initializes position buffers on first pass

3. **Frame Graph Performance** ✅ IMPROVED
   - Eliminated redundant node creation every frame
   - Optimized swapchain image resource caching  
   - Reduced debug spam from thousands of barrier logs per frame
   - Frame graph now compiles only when necessary

### Current Status
- ✅ **Rendering**: 80,000 entities displaying with compute movement
- ✅ **Performance**: Optimized frame graph execution  
- ✅ **Controls**: Mouse click entity spawning works
- ⚠️ **Camera**: System registers input but viewport doesn't update
- ⚠️ **Stability**: Occasional screen flashing persists

### Next Steps
- ❌ **Camera Viewport Integration**: Debug why camera transforms don't affect rendering
- ❌ **Frame Graph Stability**: Investigate remaining compilation triggers causing flashing
- ❌ Additional render passes (shadows, post-processing) need node implementation
- ❌ Multi-queue async compute integration pending