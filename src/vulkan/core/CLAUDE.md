# Vulkan Core Infrastructure

**Purpose**: Foundational Vulkan infrastructure providing device management, function loading, queue abstraction, RAII resource management, and utilities for the Fractalia2 engine.

## File/Folder Hierarchy

```
vulkan/core/
├── vulkan_constants.h          # System constants and limits
├── vulkan_function_loader.*    # Vulkan API function pointer management
├── vulkan_context.*            # Instance, device, and queue family detection
├── queue_manager.*             # Centralized queue and command pool abstraction
├── vulkan_manager_base.h       # Base class with cached function access
├── vulkan_raii.*               # RAII wrappers for automatic resource cleanup
├── vulkan_utils.*              # Utility functions for common Vulkan operations
├── vulkan_sync.*               # Synchronization object management
├── vulkan_swapchain.*          # Swapchain and framebuffer management
└── QUEUE_SYSTEM_OVERVIEW.md    # Queue system implementation documentation
```

## Inputs & Outputs (by component)

### vulkan_constants.h
- **Input**: None (compile-time constants)
- **Output**: `MAX_FRAMES_IN_FLIGHT=2`, timeout constants, GPU entity size, cache limits, memory thresholds
- **Usage**: System-wide constants for buffer sizing, timeouts, and configuration

### vulkan_function_loader.*
- **Input**: SDL_Window*, VkInstance, VkDevice, VkPhysicalDevice
- **Output**: Loaded Vulkan function pointers organized by loading phase
- **API Surface**: `initialize()`, `setInstance()`, `setDevice()`, `loadPostInstanceFunctions()`, `loadPostDeviceFunctions()`
- **Function Groups**: Instance management, device operations, memory, pipelines, synchronization, commands
- **Loading Phases**: pre-instance → post-instance → post-device

### vulkan_context.*
- **Input**: SDL_Window* (for surface creation)
- **Output**: VkInstance, VkSurfaceKHR, VkPhysicalDevice, VkDevice, queue handles, QueueFamilyIndices
- **API Surface**: `initialize()`, getters for all Vulkan objects, `findQueueFamilies()`, capability queries
- **Queue Detection**: Graphics (required), Present (required), Compute (required), Transfer (optional with fallback)
- **Dependencies**: VulkanFunctionLoader (internal), SDL3

### queue_manager.*
- **Input**: VulkanContext
- **Output**: Specialized command pools, frame command buffers, transfer commands with RAII lifecycle
- **API Surface**: Queue access, command pool access, command buffer allocation, transfer command management
- **Command Pool Types**: Graphics (persistent+reset), Compute (transient), Transfer (one-time)
- **Telemetry**: Submission counts, transfer command lifecycle tracking, peak usage monitoring
- **Transfer Commands**: RAII TransferCommand struct with fence and automatic cleanup

### vulkan_manager_base.h
- **Input**: VulkanContext*
- **Output**: Cached wrapper methods for common Vulkan operations
- **API Surface**: Pipeline creation wrappers, command recording helpers, resource destruction
- **Pattern**: Eliminates repetitive `context->getLoader().vkFunction()` calls
- **Usage**: Base class for pipeline managers in vulkan/pipelines/

### vulkan_raii.*
- **Input**: VkHandle, VulkanContext*
- **Output**: RAII-wrapped Vulkan objects with automatic cleanup
- **Resource Types**: ShaderModule, Pipeline, PipelineLayout, DescriptorSetLayout, DescriptorPool, RenderPass, Semaphore, Fence, CommandPool, Buffer, Image, ImageView, DeviceMemory, Framebuffer, PipelineCache, QueryPool, Instance, Device, SurfaceKHR, DebugUtilsMessengerEXT
- **API Pattern**: `vulkan_raii::Type`, factory functions `make_type()`, `create_type()`, `get()`, `release()`, `reset()`
- **Cleanup Safety**: Explicit `cleanupBeforeContextDestruction()` methods prevent use-after-free

### vulkan_utils.*
- **Input**: VulkanFunctionLoader, device handles, resource specifications
- **Output**: Created Vulkan resources, utility operation results
- **Functions**: Memory type finding, buffer/image creation, shader modules, command buffer helpers, image transitions, buffer copies, descriptor set updates, synchronization helpers, command submission
- **Cross-Domain Usage**: Used by services/, resources/, monitoring/, pipelines/

### vulkan_sync.*
- **Input**: VulkanContext, MAX_FRAMES_IN_FLIGHT
- **Output**: RAII-managed synchronization objects for frame overlap
- **Objects**: Image available semaphores, render finished semaphores, compute finished semaphores, in-flight fences, compute fences
- **Responsibility**: Pure synchronization management (no command buffers - handled by QueueManager)
- **Frame Support**: Supports 2-frame overlap with proper synchronization

### vulkan_swapchain.*
- **Input**: VulkanContext, SDL_Window*, VkRenderPass
- **Output**: VkSwapchainKHR, swapchain images/views, MSAA resources, framebuffers
- **API Surface**: `initialize()`, `recreate()`, `createFramebuffers()`, getters for all swapchain resources
- **MSAA Support**: Pre-allocated MSAA color attachment with automatic memory management
- **Recreation**: Handles window resize and surface invalidation

## Peripheral Dependencies

### Upstream Dependencies
- **SDL3**: Window management and surface creation
- **Vulkan SDK**: Core Vulkan API
- **System**: Platform-specific Vulkan loader

### Downstream Consumers
- **vulkan/pipelines/**: Pipeline managers inherit from VulkanManagerBase, use VulkanContext
- **vulkan/services/**: High-level services access queues via QueueManager, use VulkanSync
- **vulkan/resources/**: Memory managers use VulkanUtils, ResourceContext wraps VulkanContext
- **vulkan/rendering/**: FrameGraph uses QueueManager command buffers and VulkanSync objects
- **vulkan/nodes/**: Render nodes access context and synchronization through services
- **vulkan/monitoring/**: Performance monitoring uses context and utilities

## Data Flow

### Initialization Chain
1. **VulkanFunctionLoader** → loads base functions → enables instance creation
2. **VulkanContext** → creates instance → loads instance functions → creates device → loads device functions
3. **QueueManager** → detects queue families → creates specialized command pools → allocates frame buffers
4. **VulkanSync** → creates synchronization objects for frame overlap
5. **VulkanSwapchain** → creates swapchain → image views → MSAA resources

### Runtime Operation
- **QueueManager** provides optimal queue selection and command buffer access
- **VulkanSync** provides frame synchronization objects
- **VulkanUtils** handles resource creation and common operations
- **VulkanSwapchain** manages presentation and handles recreation

## Key Notes

### Performance Optimizations
- **Queue Abstraction**: QueueManager provides optimal queue selection with automatic dedicated queue detection and fallback
- **Command Pool Specialization**: Graphics (persistent), Compute (transient), Transfer (one-time) optimized for usage patterns
- **RAII Resource Management**: Zero-cost abstractions with automatic cleanup prevent leaks
- **Cached Function References**: VulkanManagerBase eliminates repeated loader lookups

### Architecture Patterns
- **Separation of Concerns**: VulkanSync handles only synchronization; QueueManager handles command buffers
- **RAII Everywhere**: Comprehensive RAII coverage for all Vulkan resources
- **Hardware Utilization**: Dedicated transfer queue support, async compute ready
- **Service Foundation**: Core provides infrastructure for higher-level service architecture

### Error Handling
- **Validation**: Debug layers and validation enabled in debug builds
- **Resource Safety**: RAII prevents resource leaks even during exceptions
- **Initialization Ordering**: Clear dependency chain ensures proper cleanup order

### Future Extensibility
- **Timeline Semaphores**: Architecture ready for timeline semaphore integration
- **Multiple Queues**: Can extend to multiple queues per family
- **Hardware Capabilities**: Runtime detection enables optimal hardware utilization

---
**Note**: When core infrastructure, queue management, or RAII patterns change, this document must be updated to reflect the modifications.