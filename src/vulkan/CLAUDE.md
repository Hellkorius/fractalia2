# Vulkan Rendering System

## Purpose
GPU-driven Vulkan rendering system for 80,000+ entities at 60 FPS with frame graph coordination, pipeline management, and monitoring.

## File Hierarchy
```
vulkan/
├── core/                        # Vulkan infrastructure (context, swapchain, sync, utils)
├── pipelines/                   # Pipeline management (compute/graphics/shader/descriptor managers)
├── rendering/                   # Frame graph coordination & ECS resource bridge
├── services/                    # High-level orchestration (frame director, command submission, sync, presentation)
├── nodes/                       # Frame graph nodes (compute movement, graphics render, presentation)
├── resources/                   # Memory allocation & resource management
└── monitoring/                  # Performance monitoring & GPU timeout detection
```

## Inputs & Outputs

**Core Flow:**
- **Input**: SDL_Window → VulkanContext → All subsystems
- **Output**: Rendered frames via coordinated compute→graphics pipeline

**Key Data Flows:**
- **Entity Pipeline**: ECS → GPUEntityManager → FrameGraph → Compute/Graphics Nodes
- **Frame Execution**: RenderFrameDirector → FrameGraph → CommandSubmissionService → GPU
- **Resource Management**: ResourceContext ↔ FrameGraph resource registry

**Major Components:**
- **core/**: VulkanContext (device/queues), VulkanSwapchain (presentation), VulkanSync (command buffers/fences)
- **pipelines/**: PipelineSystemManager (unified interface), specialized managers for compute/graphics/shaders/descriptors
- **rendering/**: FrameGraph (dependency resolution), FrameGraphResourceRegistry (ECS bridge)
- **services/**: RenderFrameDirector (orchestration), CommandSubmissionService (GPU submission), sync/presentation services
- **nodes/**: EntityComputeNode (movement), EntityGraphicsNode (rendering), SwapchainPresentNode (presentation)
- **resources/**: ResourceContext (memory allocation), CommandExecutor (async transfers)
- **monitoring/**: GPUMemoryMonitor, GPUTimeoutDetector, ComputeStressTester

## Peripheral Dependencies
- **Upstream**: SDL3, Vulkan SDK, ECS system (gpu_entity_manager, camera_system), flecs.h
- **Downstream**: vulkan_renderer.cpp (main orchestrator), movement/camera systems

## Key Notes
- **Initialization**: VulkanFunctionLoader → VulkanContext → Services (strict order)
- **Frame Overlap**: MAX_FRAMES_IN_FLIGHT=2 with fence synchronization
- **Performance**: Pipeline caching, O(n) barriers, 128-byte entity layout, async compute
- **Recovery**: Swapchain recreation, GPU timeout auto-recovery, error propagation
- **Threading**: Single-threaded design, GPU timeline via fences

If any referenced files or APIs change, this document must be updated accordingly.