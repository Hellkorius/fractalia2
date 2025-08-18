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
docs/                           # Project documentation
├── Build.md                    # Build instructions
├── Controls.md                 # User controls
├── architecture.md             # System architecture
├── hierarchy.md                # Component hierarchy
└── rendergraph.md              # Render graph design
src/
├── main.cpp                    # Application entry
├── vulkan_renderer.*           # Master renderer, frame loop
├── vulkan/                     # Vulkan subsystems
│   ├── core/                   # Core Vulkan infrastructure
│   │   ├── vulkan_context.*    # Instance, device, queue management
│   │   ├── vulkan_function_loader.* # Vulkan function loading
│   │   ├── vulkan_swapchain.*  # Swapchain, MSAA, framebuffers
│   │   ├── vulkan_sync.*       # Fences, semaphores
│   │   ├── vulkan_utils.*      # Vulkan utilities
│   │   └── vulkan_constants.h  # Vulkan constants
│   ├── pipelines/              # Pipeline management systems
│   │   ├── compute_pipeline_manager.* # Compute pipeline caching & dispatch
│   │   ├── graphics_pipeline_manager.* # Graphics pipeline state objects
│   │   ├── pipeline_system_manager.* # AAA pipeline system coordinator
│   │   ├── pipeline_system_simple.h # Simple pipeline system
│   │   ├── shader_manager.*    # SPIR-V loading & hot-reload
│   │   ├── descriptor_layout_manager.* # Descriptor set layout caching
│   │   └── vulkan_pipeline.*   # Pipeline utilities
│   ├── resources/              # Memory and resource management
│   │   ├── resource_context.*  # Buffer/memory manager
│   │   └── command_executor.*  # Command buffer execution
│   ├── rendering/              # Frame graph and rendering coordination
│   │   ├── frame_graph.*       # Render graph coordinator
│   │   └── frame_graph_resource_registry.* # Resource registration with frame graph
│   ├── services/               # High-level services
│   │   ├── render_frame_director.* # Frame execution orchestration
│   │   ├── command_submission_service.* # GPU queue submission management
│   │   ├── gpu_synchronization_service.* # GPU synchronization primitives
│   │   └── presentation_surface.* # Swapchain and presentation management
│   ├── nodes/                  # Render graph nodes
│   │   ├── entity_compute_node.* # Compute movement
│   │   ├── entity_graphics_node.* # Graphics rendering
│   │   └── swapchain_present_node.* # Presentation
│   └── monitoring/             # Performance monitoring and debugging
│       ├── gpu_memory_monitor.* # GPU memory usage monitoring
│       ├── gpu_timeout_detector.* # GPU timeout detection
│       └── compute_stress_tester.* # Compute stress testing
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
    // Cache Line 1 (bytes 0-63) - HOT DATA: All frequently accessed in compute shaders
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, stateTimer, entityState
    glm::vec4 color;              // 16 bytes - RGBA color
    
    // Cache Line 2 (bytes 64-127) - COLD DATA: Rarely accessed
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
}; // 128 bytes total (2 cache lines optimized)
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

### Pipeline System
- **PipelineSystemManager**: Unified interface for all pipeline operations
- **ComputePipelineManager**: Compute pipeline caching with dispatch optimization
- **GraphicsPipelineManager**: Graphics pipeline state objects with LRU caching
- **ShaderManager**: SPIR-V loading, compilation, and hot-reload support
- **DescriptorLayoutManager**: Descriptor set layout caching and pool management

## Current Status
- **Entities**: 80,000 at 60 FPS
- **Movement**: 600-frame interpolated random walk cycles
- **Camera**: WASD + mouse wheel + rotation controls
- **Stability**: Zero crashes, optimized barrier insertion, uniform buffer caching
- **Performance**: Linear frame graph barriers, dirty tracking, batch command resets

## Planned Extensions
- **Multi-queue async compute**: Overlapped compute/graphics execution
- **GPU memory monitoring**: Dynamic quality scaling based on usage
- **Device lost recovery**: Robust error handling for driver issues
- **Frame timing metrics**: Performance visibility and bottleneck identification
- **Command buffer pooling**: Enhanced driver optimization beyond current batch resets