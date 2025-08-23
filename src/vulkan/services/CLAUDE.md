# CLAUDE.md

## src/vulkan/services/ (High-level Vulkan coordination services that orchestrate frame execution, error recovery, and GPU synchronization)

### command_submission_service.h
**Inputs:** Frame indices, execution results from FrameGraph, VulkanContext, VulkanSync, VulkanSwapchain, QueueManager.  
**Outputs:** SubmissionResult with success/failure status and swapchain recreation flags.  
**Function:** Defines service interface for async compute and graphics command submission with presentation coordination.

### command_submission_service.cpp
**Inputs:** Current frame data, command buffers from QueueManager, synchronization primitives from VulkanSync.  
**Outputs:** Submitted GPU work to compute and graphics queues, presentation requests to present queue.  
**Function:** Implements parallel async compute/graphics submission pattern where compute calculates frame N+1 while graphics renders frame N.

### error_recovery_service.h
**Inputs:** RenderFrameResult indicating failure, frame timing data, Flecs world reference.  
**Outputs:** Recovery success status and retry frame results after swapchain recreation.  
**Function:** Defines error recovery interface that attempts swapchain recreation when frame execution fails.

### error_recovery_service.cpp
**Inputs:** Failed frame results, PresentationSurface for swapchain management, RenderFrameDirector for retry attempts.  
**Outputs:** Boolean recovery success, retried frame execution through RenderFrameDirector.  
**Function:** Analyzes frame failures and attempts recovery via proactive swapchain recreation with frame retry logic.

### frame_state_manager.h
**Inputs:** Frame indices, compute/graphics usage flags per frame.  
**Outputs:** Lists of VkFences that need waiting based on previous frame usage patterns.  
**Function:** Tracks which GPU operations were used per frame to determine required fence synchronization.

### frame_state_manager.cpp
**Inputs:** Per-frame usage state updates from rendering pipeline, VulkanSync fence references.  
**Outputs:** Filtered fence arrays containing only fences that require waiting based on usage history.  
**Function:** Maintains circular buffer of frame states to optimize fence waiting by skipping unused operations.

### gpu_synchronization_service.h
**Inputs:** VulkanContext for device access, frame indices for fence selection.  
**Outputs:** VkFence handles for compute/graphics operations, timeout results from fence waits.  
**Function:** Manages per-frame compute and graphics fences using RAII wrappers with robust timeout handling.

### gpu_synchronization_service.cpp
**Inputs:** Frame-specific fence wait requests, swapchain recreation signals requiring all-frame synchronization.  
**Outputs:** Synchronized GPU state via fence waits, reset fence states after swapchain recreation.  
**Function:** Provides robust fence waiting with timeout handling and critical fence reset logic to prevent corruption across swapchain recreation.

### presentation_surface.h
**Inputs:** VulkanContext, VulkanSwapchain, GraphicsPipelineManager, GPUSynchronizationService via constructor dependency injection.  
**Outputs:** SurfaceAcquisitionResult containing acquired image indices and recreation flags.  
**Function:** Coordinates swapchain image acquisition with recreation detection and framebuffer resize handling. Uses constructor-based initialization pattern.

### presentation_surface.cpp
**Inputs:** Current frame index, framebuffer resize events, dependencies injected via constructor.  
**Outputs:** Acquired swapchain images with proper timeout handling, recreated swapchain resources.  
**Function:** Implements constructor-based service pattern with full dependency injection. Handles swapchain image acquisition with timeout protection and orchestrates full swapchain recreation including pipeline cache regeneration.

### render_frame_director.h
**Inputs:** All Vulkan subsystem managers, ECS world reference, frame timing data, resource IDs.  
**Outputs:** RenderFrameResult containing execution success and acquired swapchain image index.  
**Function:** Master frame orchestration service that coordinates image acquisition, frame graph setup, node configuration, and execution.

### render_frame_director.cpp
**Inputs:** Frame counters, timing data, Flecs world, swapchain images, GPU entity and resource managers.  
**Outputs:** Configured and executed frame graph with proper node setup, updated descriptor sets after swapchain recreation.  
**Function:** Implements complete frame direction flow from image acquisition through frame graph execution with dynamic swapchain image resolution and comprehensive swapchain recreation handling.