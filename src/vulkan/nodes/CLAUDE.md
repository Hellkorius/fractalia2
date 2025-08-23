# Vulkan Nodes

## Directory Overview
(Streamlined frame graph node implementations with unified lifecycle and push constants)

### Simplified Node Architecture
- **Unified Lifecycle**: Nodes use only `execute()` and optional `onFirstUse()` methods
- **Unified Push Constants**: All nodes use `NodePushConstants` structure with flexible parameters
- **Consolidated Debug**: Replaced individual counters with `FrameGraphDebug` utilities

### Files

**entity_compute_node.h**
- **Inputs**: Entity buffer resource IDs, ComputePipelineManager, GPUEntityManager, GPUTimeoutDetector
- **Outputs**: Modified entity buffer with updated movement parameters, compute shader barriers for graphics synchronization
- **Function**: Orchestrates GPU compute workloads for entity movement using adaptive chunked dispatching with unified `NodePushConstants`.

**entity_compute_node.cpp**
- **Inputs**: Command buffer, frame timing data (time, deltaTime), entity count from GPUEntityManager
- **Outputs**: Executed compute dispatches with memory barriers, unified push constants with `param1` for chunk offsets
- **Function**: Implements chunked compute execution with GPU health monitoring using simplified lifecycle pattern.

**entity_graphics_node.h**
- **Inputs**: Entity/position buffer resource IDs, GraphicsPipelineManager, VulkanSwapchain, ResourceCoordinator, GPUEntityManager
- **Outputs**: Rendered frame to swapchain image, updated uniform buffers, graphics pipeline state
- **Function**: Manages instanced rendering pipeline with camera matrix updates and descriptor set binding.

**entity_graphics_node.cpp**
- **Inputs**: Command buffer, swapchain framebuffers, camera matrices from CameraService, entity count
- **Outputs**: Render pass execution with MSAA, instanced draw calls, uniform buffer updates with dirty tracking
- **Function**: Executes graphics rendering with viewport management, dynamic descriptor binding, and optimized uniform buffer caching.

**physics_compute_node.h**
- **Inputs**: Entity/position buffer resource IDs, ComputePipelineManager, GPUEntityManager, GPUTimeoutDetector
- **Outputs**: Updated position and current position buffers, spatial map clearing workgroups, compute barriers
- **Function**: Handles spatial hash collision detection compute workloads with adaptive dispatching and chunk management.

**physics_compute_node.cpp**
- **Inputs**: Command buffer, frame timing, entity positions, spatial map size requirements
- **Outputs**: Physics compute dispatches with unified `NodePushConstants`, memory barriers for data consistency, consolidated debug logging
- **Function**: Implements physics simulation with spatial map management using unified push constants (`param1` for entityOffset) and streamlined debug utilities.

**swapchain_present_node.h**
- **Inputs**: Color target resource ID, VulkanSwapchain, current swapchain image index
- **Outputs**: Presentation dependency declarations, swapchain image resource bindings
- **Function**: Declares presentation dependencies for proper frame graph synchronization without direct command buffer operations.

**swapchain_present_node.cpp**
- **Inputs**: Frame graph context, swapchain state, image index validation
- **Outputs**: Dependency validation results, presentation readiness confirmation
- **Function**: Validates presentation dependencies and image bounds for queue-level presentation handled by frame graph execution.