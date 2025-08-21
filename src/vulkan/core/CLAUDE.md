# Vulkan Core Infrastructure

**Purpose**: Foundation layer providing device management, function loading, queue abstraction, RAII resource management, and utilities for all Vulkan operations in Fractalia2.

## Architecture Overview

```
VulkanFunctionLoader → VulkanContext → QueueManager → VulkanSync/VulkanSwapchain
         ↓                ↓              ↓
    All API calls    Device/Queues   Command Buffers
```

**Core Pattern**: Progressive initialization with dependency injection through VulkanContext.

## Key Components

### VulkanContext - Central Hub
- **Role**: Master coordinator owning instance, device, queues, and function loader
- **Initialization Chain**: SDL window → instance → device → queues → function loading
- **Queue Detection**: Graphics (required), Present (required), Compute (required), Transfer (optional fallback)
- **API**: `getInstance()`, `getDevice()`, `getGraphicsQueue()`, `getLoader()`, `hasDedicatedComputeQueue()`

### VulkanFunctionLoader - API Gateway
- **Role**: Centralized Vulkan function pointer management with progressive loading
- **Loading Phases**: Pre-instance → Post-instance → Post-device (180+ functions)
- **Function Groups**: Instance, device, memory, buffers, images, pipelines, synchronization, commands
- **Usage Pattern**: `const auto& vk = context->getLoader(); vk.vkCreateBuffer(...)`

### QueueManager - Command Execution
- **Role**: Queue abstraction with specialized command pools and optimal command buffer allocation
- **Command Pool Types**:
  - Graphics: Persistent buffers with reset capability
  - Compute: Transient buffers for short dispatches
  - Transfer: One-time buffers with RAII lifecycle
- **Frame Management**: Pre-allocated graphics/compute buffers per frame (MAX_FRAMES_IN_FLIGHT=2)
- **Transfer Commands**: RAII TransferCommand with fence and automatic cleanup

### RAII System (vulkan_raii.h)
- **Role**: Automatic resource cleanup with move semantics for all Vulkan objects
- **Template Pattern**: `VulkanHandle<VkType, Deleter>` with VulkanContext* for cleanup
- **Safety**: `cleanupBeforeContextDestruction()` prevents use-after-free
- **Covered Types**: Pipeline, Buffer, Image, Semaphore, Fence, DescriptorSet, etc.

### VulkanManagerBase - Pipeline Base Class
- **Role**: Base class for pipeline managers eliminating repetitive function calls
- **Cached Access**: Stores loader and device references for performance
- **Convenience Methods**: `createGraphicsPipelines()`, `cmdBindPipeline()`, `cmdDispatch()`
- **Used By**: All pipeline managers in vulkan/pipelines/

## Data Flow Patterns

### Initialization Sequence
1. **VulkanFunctionLoader** loads base functions
2. **VulkanContext** creates instance → loads instance functions → creates device → loads device functions
3. **QueueManager** detects queue families → creates command pools → allocates frame buffers
4. **VulkanSync/VulkanSwapchain** create synchronization and presentation resources

### Runtime Operation
- **Function Access**: `context->getLoader().vkFunction()` or VulkanManagerBase wrappers
- **Command Buffers**: QueueManager provides optimal allocation (graphics/compute per-frame, transfer on-demand)
- **Resource Management**: RAII wrappers ensure automatic cleanup with proper destruction order
- **Queue Selection**: QueueManager handles fallback (transfer → graphics) and dedicated queue detection

## Critical Architecture Patterns

### Local Caching Pattern (Performance)
```cpp
const auto& vk = context->getLoader();
const VkDevice device = context->getDevice();
// Multiple vk.vkFunction() calls without repeated context lookups
```

### RAII Factory Pattern (Safety)
```cpp
auto buffer = vulkan_raii::make_buffer(bufferHandle, context);
// Automatic cleanup on scope exit, move semantics supported
```

### Queue Abstraction Pattern (Flexibility)
```cpp
VkQueue queue = queueManager->getTransferQueue(); // Automatic fallback to graphics
if (queueManager->hasDedicatedTransferQueue()) { /* optimize */ }
```

### Progressive Function Loading (Initialization)
```cpp
loader->initialize(window);           // Base functions
context->createInstance();            // → loadPostInstanceFunctions()
context->createDevice();              // → loadPostDeviceFunctions()
```

## Integration Points

### Upstream Dependencies
- **SDL3**: Window management and surface creation
- **Vulkan SDK**: API headers and validation layers

### Downstream Consumers
- **vulkan/pipelines/**: Pipeline managers inherit VulkanManagerBase
- **vulkan/services/**: High-level services access queues and synchronization
- **vulkan/resources/**: Memory managers use VulkanUtils and RAII wrappers
- **vulkan/rendering/**: Frame graph uses QueueManager command buffers

## Performance Considerations

- **Queue Optimization**: Dedicated compute/transfer queues when available, automatic fallback
- **Command Pool Specialization**: Different pool configurations optimize for usage patterns
- **Function Caching**: VulkanManagerBase eliminates repeated loader access
- **RAII Zero-Cost**: Template-based wrappers with move semantics, no runtime overhead
- **Frame Overlap**: Pre-allocated command buffers support MAX_FRAMES_IN_FLIGHT=2

---
**Note**: Core infrastructure changes require updates to all downstream Vulkan modules. Always verify RAII cleanup order and queue fallback behavior.