# Vulkan Core Infrastructure

Device management, function loading, synchronization, swapchain management, utilities.

## Files
```
vulkan_constants.h          # Constants: MAX_FRAMES_IN_FLIGHT=2, timeouts, sizes
vulkan_function_loader.*    # Vulkan function pointer loading  
vulkan_context.*            # Instance, device, queue, surface management
vulkan_manager_base.h       # Base class: cached loader/device refs, wrapper methods
vulkan_raii.*               # RAII wrappers for automatic Vulkan resource cleanup
vulkan_utils.*              # Buffer/image creation, memory utils, command helpers
vulkan_sync.*               # Command pools, semaphores, fences (RAII-managed)
vulkan_swapchain.*          # Swapchain, MSAA, framebuffers
```

## Components

### vulkan_constants.h
`MAX_FRAMES_IN_FLIGHT=2`, `FENCE_TIMEOUT_*`, `GPU_ENTITY_SIZE=128`

### vulkan_function_loader.*
**In:** SDL_Window*, VkInstance, VkDevice, VkPhysicalDevice
**Out:** All Vulkan function pointers
**API:** `initialize()`, `setInstance()`, `setDevice()`, `loadPostInstanceFunctions()`, `loadPostDeviceFunctions()`
**Loading:** pre-instance → post-instance → post-device

### vulkan_manager_base.h
**In:** VulkanContext*
**Out:** Cached wrapper methods for Vulkan functions
**API:** `createGraphicsPipelines()`, `createComputePipelines()`, `createPipelineLayout()`, `createRenderPass()`, `cmdBindPipeline()`, `cmdDispatch()`, `cmdPipelineBarrier()`
**Optimization:** Eliminates 270+ `context->getLoader().vkFunction()` calls, reduces code by 40%

### vulkan_context.*
**In:** SDL_Window*, VulkanFunctionLoader
**Out:** VkInstance, VkSurfaceKHR, VkPhysicalDevice, VkDevice, VkQueue handles, QueueFamilyIndices
**API:** `initialize()`, `getInstance()`, `getDevice()`, `getGraphicsQueue()`, `findQueueFamilies()`, `getLoader()`
**Flow:** instance → surface → physical device → logical device → queues

### vulkan_utils.*
**In:** VulkanFunctionLoader, device handles, resource specs
**Out:** VkBuffer, VkImage, VkImageView, VkShaderModule, memory operations
**API:** `findMemoryType()`, `createBuffer()`, `createImage()`, `createImageView()`, `createShaderModule()`, `beginSingleTimeCommands()`, `transitionImageLayout()`, `copyBuffer()`, `writeDescriptorSets()`

### vulkan_raii.*
**In:** VkHandle, VulkanContext*
**Out:** RAII-wrapped Vulkan objects with automatic cleanup
**API:** `vulkan_raii::ShaderModule`, `vulkan_raii::Semaphore`, `vulkan_raii::Fence`, `make_*()` factories
**Pattern:** Template wrappers with move semantics, explicit `cleanupBeforeContextDestruction()`
**Deleters:** Context-aware cleanup, null-safe destruction, proper Vulkan API calls

### vulkan_sync.*
**In:** VulkanContext, MAX_FRAMES_IN_FLIGHT
**Out:** VkCommandPool, VkCommandBuffer arrays, RAII semaphores/fences
**API:** `initialize()`, `getCommandPool()`, `getCommandBuffers()`, `get*Semaphore(index)`, `get*Fence(index)`, `resetCommandBuffersForFrame()`, `resetAllCommandBuffers()`
**Sync:** RAII-managed `imageAvailableSemaphores`, `renderFinishedSemaphores`, `computeFinishedSemaphores`, `inFlightFences`, `computeFences`
**Access:** Individual element getters prevent temporary vector indexing issues

### vulkan_swapchain.*
**In:** VulkanContext, SDL_Window*, VkRenderPass
**Out:** VkSwapchainKHR, VkImage array, VkImageView array, MSAA resources, VkFramebuffer array
**API:** `initialize()`, `recreate()`, `createFramebuffers()`, `getSwapchain()`, `getImages()`, `getImageViews()`, `getFramebuffers()`, `getMSAAColorImage()`, `getMSAAColorImageView()`

## Dependencies

**Init:** VulkanFunctionLoader ← SDL_Window → VulkanContext → VulkanSync + VulkanSwapchain
**Runtime:** All modules → VulkanFunctionLoader + VulkanContext; Pipeline managers → VulkanManagerBase

## External Dependencies

**Up:** SDL3, Vulkan SDK
**Down:** pipelines/ (inherit VulkanManagerBase), services/, rendering/, resources/, nodes/, monitoring/
**Interface:** SDL window events, ECS SDL_Window handle, shader/memory systems use VulkanUtils

## Notes

**Init Order:** VulkanFunctionLoader → VulkanContext → VulkanSync/VulkanSwapchain
**Function Loading:** Three-phase (pre-instance → post-instance → post-device), VulkanManagerBase caches refs
**Memory:** VulkanUtils finds memory types, RAII wrappers provide auto-cleanup for sync objects
**RAII:** ShaderManager + VulkanSync use RAII wrappers, explicit cleanup before context destruction
**Performance:** VulkanManagerBase eliminates 270+ calls (-40% code), command buffer reset optimizations, pre-allocated MSAA
**Errors:** bool returns, validation layers in debug, clean failure states