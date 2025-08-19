# Vulkan Core Infrastructure

Core Vulkan infrastructure providing device management, function loading, centralized queue abstraction, synchronization, swapchain management, and utilities.

## Files
```
vulkan_constants.h          # Constants: MAX_FRAMES_IN_FLIGHT=2, timeouts, sizes
vulkan_function_loader.*    # Vulkan function pointer loading system
vulkan_context.*            # Instance, device, queue family detection, surface management
queue_manager.*             # Centralized queue abstraction with specialized command pools
vulkan_manager_base.h       # Base class: cached loader/device refs, wrapper methods
vulkan_raii.*               # RAII wrappers for automatic Vulkan resource cleanup
vulkan_utils.*              # Buffer/image creation, memory utils, command helpers
vulkan_sync.*               # Pure synchronization object management (semaphores/fences)
vulkan_swapchain.*          # Swapchain, MSAA, framebuffers
```

## Core Components

### vulkan_constants.h
**Data:** `MAX_FRAMES_IN_FLIGHT=2`, `FENCE_TIMEOUT_IMMEDIATE/FRAME`, `GPU_ENTITY_SIZE=128`

### vulkan_function_loader.*
**In:** SDL_Window*, VkInstance, VkDevice, VkPhysicalDevice
**Out:** All Vulkan function pointers organized by loading phase
**API:** `initialize()`, `setInstance()`, `setDevice()`, `loadPostInstanceFunctions()`, `loadPostDeviceFunctions()`
**Loading phases:** pre-instance → post-instance → post-device
**Function groups:** Instance, physical device, surface, device management, memory, buffers, images, pipelines, sync, commands

### vulkan_context.*
**In:** SDL_Window*
**Out:** VkInstance, VkSurfaceKHR, VkPhysicalDevice, VkDevice, queue handles, QueueFamilyIndices
**API:** `initialize()`, `getInstance()`, `getDevice()`, `getGraphicsQueue()`, `getComputeQueue()`, `getTransferQueue()`, `findQueueFamilies()`, `hasDedicatedTransferQueue()`, `hasDedicatedComputeQueue()`
**Queue detection:** Graphics (required), Present (required), Compute (required), Transfer (optional with graphics fallback)
**Capability queries:** Dedicated queue detection for optimal hardware utilization

### queue_manager.*
**In:** VulkanContext
**Out:** Specialized command pools, frame command buffers, transfer commands, queue telemetry
**API:** `getGraphicsQueue()`, `getComputeQueue()`, `getTransferQueue()`, `getCommandPool(type)`, `getGraphicsCommandBuffer(frameIndex)`, `allocateTransferCommand()`, `getTelemetry()`
**Command pools:** Graphics (persistent+reset), Compute (transient), Transfer (one-time)
**Transfer commands:** RAII TransferCommand with fence, automatic queue selection
**Telemetry:** Submission counts, transfer command lifecycle tracking

### vulkan_manager_base.h
**In:** VulkanContext*
**Out:** Cached wrapper methods for common Vulkan operations
**API:** `createGraphicsPipelines()`, `createComputePipelines()`, `createPipelineLayout()`, `createRenderPass()`, `cmdBindPipeline()`, `cmdDispatch()`, `cmdPipelineBarrier()`
**Optimization:** Eliminates repetitive `context->getLoader().vkFunction()` patterns

### vulkan_raii.*
**In:** VkHandle, VulkanContext*
**Out:** RAII-wrapped Vulkan objects with automatic cleanup
**Types:** ShaderModule, Pipeline, PipelineLayout, DescriptorSetLayout, DescriptorPool, RenderPass, Semaphore, Fence, CommandPool, Buffer, Image, ImageView, DeviceMemory, Framebuffer, PipelineCache, QueryPool
**API:** `vulkan_raii::Type`, `make_type()`, `create_type()` factories, `get()`, `release()`, `reset()`
**Pattern:** Template-based RAII wrappers with move semantics, context-aware deleters
**Cleanup:** Explicit `cleanupBeforeContextDestruction()` for destruction order safety

### vulkan_utils.*
**In:** VulkanFunctionLoader, device handles, resource specifications
**Out:** Created Vulkan resources and utility operations
**API:** `findMemoryType()`, `createBuffer()`, `createImage()`, `createImageView()`, `createShaderModule()`, `beginSingleTimeCommands()`, `transitionImageLayout()`, `copyBuffer()`, `writeDescriptorSets()`, `submitCommands()`
**Cross-domain:** Utility functions used by services/, resources/, monitoring/

### vulkan_sync.*
**In:** VulkanContext, MAX_FRAMES_IN_FLIGHT
**Out:** RAII-managed synchronization objects
**API:** `initialize()`, `getImageAvailableSemaphore(index)`, `getRenderFinishedSemaphore(index)`, `getComputeFinishedSemaphore(index)`, `getInFlightFence(index)`, `getComputeFence(index)`
**Sync objects:** Image available semaphores, render finished semaphores, compute finished semaphores, in-flight fences, compute fences
**Responsibility:** Pure synchronization object management (no command buffers)

### vulkan_swapchain.*
**In:** VulkanContext, SDL_Window*, VkRenderPass
**Out:** VkSwapchainKHR, swapchain images/views, MSAA resources, framebuffers
**API:** `initialize()`, `recreate()`, `createFramebuffers()`, `getSwapchain()`, `getImages()`, `getImageViews()`, `getFramebuffers()`, `getMSAAColorImage()`, `getMSAAColorImageView()`
**MSAA:** Pre-allocated color attachment with automatic memory management

## Data Flow

### Initialization
1. **VulkanFunctionLoader** loads base functions → instance creation
2. **VulkanContext** creates instance → loads instance functions → creates device → loads device functions
3. **QueueManager** detects queue families → creates specialized command pools → allocates frame command buffers
4. **VulkanSync** creates synchronization objects
5. **VulkanSwapchain** creates swapchain → image views → MSAA resources

### Runtime
1. **QueueManager** provides optimal queue selection and command buffer access
2. **VulkanSync** provides frame synchronization objects
3. **VulkanUtils** handles resource creation and command operations
4. **VulkanSwapchain** manages presentation and framebuffer recreation

## Dependencies

**Initialization chain:** VulkanFunctionLoader → VulkanContext → QueueManager + VulkanSync → VulkanSwapchain
**Runtime dependencies:** All modules use VulkanContext + VulkanFunctionLoader; QueueManager for commands; VulkanUtils for operations

## External Interface

**Up:** SDL3 (window/surface), Vulkan SDK (API)
**Down:** pipelines/ (VulkanManagerBase inheritance), services/ (queue access), rendering/ (command buffers), resources/ (utilities), monitoring/ (telemetry)

## Architecture Notes

**Queue abstraction:** QueueManager provides centralized queue management with automatic fallback
**Separation of concerns:** VulkanSync handles only synchronization; QueueManager handles command buffers
**RAII patterns:** Comprehensive RAII coverage prevents resource leaks
**Performance:** Cached function references, specialized command pools, optimal queue utilization
**Hardware utilization:** Dedicated transfer queue support, async compute ready, comprehensive capability detection