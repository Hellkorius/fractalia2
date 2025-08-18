# Vulkan Core Infrastructure Documentation

## Purpose
Foundational Vulkan infrastructure providing device management, function loading, synchronization primitives, swapchain management, and common utilities. All other Vulkan subsystems depend on these core components.

## File/Folder Hierarchy
```
vulkan/core/
├── vulkan_constants.h          # Global Vulkan constants and configuration
├── vulkan_function_loader.*    # Centralized Vulkan function pointer management  
├── vulkan_context.*            # Instance, device, queue, and surface management
├── vulkan_utils.*              # Common Vulkan operation utilities
├── vulkan_sync.*               # Synchronization objects and command pools
└── vulkan_swapchain.*          # Swapchain, MSAA, and framebuffer management
```

## Inputs & Outputs (by Component)

### vulkan_constants.h
**Inputs:** None (compile-time constants)
**Outputs:** 
- `MAX_FRAMES_IN_FLIGHT = 2` (frame overlap control)
- `FENCE_TIMEOUT_*` constants (synchronization timeouts)
- `GPU_ENTITY_SIZE = 128` (entity buffer sizing)

### vulkan_function_loader.* 
**Inputs:**
- SDL_Window* (for SDL Vulkan integration)
- VkInstance handle (from VulkanContext)
- VkDevice + VkPhysicalDevice handles (from VulkanContext)

**Outputs:**
- Loaded function pointers for all Vulkan API calls
- Centralized function access for entire Vulkan subsystem
- Three-phase loading: pre-instance → post-instance → post-device

**API Surface:**
- `initialize(SDL_Window*)` → `bool`
- `setInstance(VkInstance)` / `setDevice(VkDevice, VkPhysicalDevice)`
- `loadPostInstanceFunctions()` / `loadPostDeviceFunctions()` → `bool`
- Direct access to all Vulkan function pointers (vk* members)

### vulkan_context.*
**Inputs:**
- SDL_Window* (for surface creation)
- VulkanFunctionLoader instance (for API access)

**Outputs:**
- VkInstance (Vulkan instance with validation layers)
- VkSurfaceKHR (rendering surface)
- VkPhysicalDevice (selected GPU)
- VkDevice (logical device)
- VkQueue handles: graphics, present, compute queues
- QueueFamilyIndices (queue family indices)

**API Surface:**
- `initialize(SDL_Window*)` → `bool`
- Getters: `getInstance()`, `getDevice()`, `getGraphicsQueue()`, etc.
- `findQueueFamilies(VkPhysicalDevice)` → `QueueFamilyIndices`
- `getLoader()` → `VulkanFunctionLoader&`

**Creation Flow:**
1. Create instance with extensions + validation
2. Create surface via SDL
3. Select suitable physical device  
4. Create logical device with queue families
5. Retrieve queue handles

### vulkan_utils.*
**Inputs:**
- VulkanFunctionLoader reference (for API access)
- Vulkan handles (device, physical device, queues, command pools)
- Resource specifications (buffer sizes, image dimensions, memory properties)

**Outputs:**
- Created Vulkan resources (buffers, images, image views, shader modules)
- Memory management operations
- Command buffer utilities for one-shot operations

**API Surface:**
- `findMemoryType()` → `uint32_t` (memory type index)
- `createBuffer()` → `bool` + VkBuffer + VkDeviceMemory
- `createImage()` → `bool` + VkImage + VkDeviceMemory  
- `createImageView()` → `VkImageView`
- `createShaderModule()` → `VkShaderModule`
- `beginSingleTimeCommands()` / `endSingleTimeCommands()`
- `transitionImageLayout()`, `copyBuffer()`, `copyBufferToImage()`
- `writeDescriptorSets()`

### vulkan_sync.*
**Inputs:**
- VulkanContext reference (for device access)
- MAX_FRAMES_IN_FLIGHT constant

**Outputs:**
- VkCommandPool (graphics command pool)
- Per-frame VkCommandBuffer arrays (graphics + compute)
- Synchronization objects: semaphores + fences for frame overlap
- Command buffer management utilities

**API Surface:**
- `initialize(VulkanContext&)` → `bool`
- Getters: `getCommandPool()`, `getCommandBuffers()`, `get*Semaphores()`, `get*Fences()`
- `resetCommandBuffersForFrame(uint32_t)` (frame-specific reset)
- `resetAllCommandBuffers()` (bulk reset optimization)

**Synchronization Objects:**
- `imageAvailableSemaphores` (swapchain → graphics)
- `renderFinishedSemaphores` (graphics → present)
- `computeFinishedSemaphores` (compute → graphics)
- `inFlightFences` + `computeFences` (CPU synchronization)

### vulkan_swapchain.*
**Inputs:**
- VulkanContext reference (for device/surface access)
- SDL_Window* (for extent calculation)
- VkRenderPass (for framebuffer creation)

**Outputs:**
- VkSwapchainKHR with optimal format/present mode
- VkImage array (swapchain images)
- VkImageView array (swapchain image views)
- MSAA color resources (VkImage + VkImageView + VkDeviceMemory)
- VkFramebuffer array (one per swapchain image)

**API Surface:**
- `initialize(VulkanContext&, SDL_Window*)` → `bool`
- `recreate(VkRenderPass)` → `bool` (window resize handling)
- `createFramebuffers(VkRenderPass)` → `bool`
- Getters: `getSwapchain()`, `getImages()`, `getImageViews()`, `getFramebuffers()`
- MSAA getters: `getMSAAColorImage()`, `getMSAAColorImageView()`

## Data Flows Between Components

### Initialization Sequence:
1. **VulkanFunctionLoader** ← SDL_Window (gets base function pointers)
2. **VulkanContext** ← VulkanFunctionLoader + SDL_Window (creates instance/device)
3. **VulkanFunctionLoader** ← instance/device handles (loads remaining functions)
4. **VulkanSync** ← VulkanContext (creates command pools/sync objects)
5. **VulkanSwapchain** ← VulkanContext + SDL_Window (creates swapchain)
6. **VulkanSwapchain** ← VkRenderPass (creates framebuffers)

### Runtime Dependencies:
- **All modules** → VulkanFunctionLoader (for Vulkan API calls)
- **All modules** → VulkanContext (for device/queue access)
- **Rendering operations** → VulkanSync (for command buffers/synchronization)
- **Presentation** → VulkanSwapchain (for framebuffers/swapchain)
- **Resource creation** → VulkanUtils (for common operations)

## Peripheral Dependencies

### Upstream Dependencies (Core consumes):
- **SDL3**: Window system integration, Vulkan surface creation
- **Vulkan SDK**: Core API definitions and validation layers

### Downstream Consumers (23+ modules):
- **pipelines/**: All pipeline managers use VulkanContext + VulkanFunctionLoader
- **services/**: RenderFrameDirector, CommandSubmissionService, GPUSynchronizationService, PresentationSurface
- **rendering/**: FrameGraph uses VulkanContext + VulkanSync
- **resources/**: ResourceContext, CommandExecutor use core infrastructure
- **nodes/**: EntityComputeNode, EntityGraphicsNode, SwapchainPresentNode use VulkanContext + VulkanFunctionLoader
- **monitoring/**: GPUMemoryMonitor, ComputeStressTester, GPUTimeoutDetector use VulkanContext
- **Main renderer**: vulkan_renderer.cpp orchestrates initialization sequence

### External Interface Points:
- **SDL integration**: Window events trigger swapchain recreation
- **ECS system**: Provides SDL_Window handle during initialization
- **Shader system**: Uses VulkanUtils for shader module creation
- **Memory system**: Uses VulkanUtils for buffer/image creation

## Key Notes

### Initialization Dependencies:
- VulkanFunctionLoader MUST be initialized before any other Vulkan operations
- VulkanContext MUST complete device creation before function loader can load device functions
- VulkanSync/VulkanSwapchain require fully initialized VulkanContext

### Function Loading Pattern:
- Three-phase loading prevents chicken-and-egg problems
- All modules access Vulkan functions through VulkanFunctionLoader (no direct linking)
- Instance functions loaded first, then device functions

### Memory Management:
- VulkanUtils provides memory type finding for buffer/image allocation
- No automatic cleanup - callers responsible for resource lifecycle
- Memory allocations use VkPhysicalDeviceMemoryProperties for optimal placement

### Performance Considerations:
- Command buffer reset optimizations in VulkanSync (per-frame vs bulk reset)
- MSAA resources pre-allocated in VulkanSwapchain
- Function pointer access is direct (no indirection overhead)

### Error Handling:
- All initialization methods return bool for success/failure
- Validation layers enabled in debug builds via VulkanContext
- Failed operations leave objects in clean state for retry/cleanup

## Update Requirements
**IMPORTANT**: If any referenced modules, APIs, or data structures change, this documentation must be updated to maintain accuracy for AI agents. Key update triggers:
- Changes to vulkan_constants.h values
- Modifications to API surfaces in header files  
- Additions/removals of Vulkan function pointers
- Changes to initialization sequences or dependencies
- Modifications to synchronization objects or command buffer management