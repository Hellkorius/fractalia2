# Vulkan Resources Module - GPU Resource Management & Memory Allocation

## Purpose
Modular Vulkan resource management providing memory allocation, buffer/image creation, descriptor management, and command execution for the GPU-driven rendering pipeline. Serves as the bridge between high-level services and low-level Vulkan operations.

## File Hierarchy
```
vulkan/resources/
├── CLAUDE.md                         # This documentation
├── resource_context.h/cpp            # Master coordinator - delegates to specialized managers
├── resource_handle.h                 # RAII resource wrapper with vulkan_raii types
├── memory_allocator.h/cpp            # VMA-compatible memory allocation + pressure monitoring
├── buffer_factory.h/cpp              # Buffer/image creation + transfer operations
├── command_executor.h/cpp            # Queue-optimal command execution via QueueManager
├── descriptor_pool_manager.h/cpp     # Descriptor pool creation + management
├── graphics_resource_manager.h/cpp   # Graphics pipeline specific resources
└── resource_utils.h/cpp              # Utility functions for common resource operations
```

## Core Input/Output Documentation

### ResourceContext (Master Coordinator)
**Input:**
- `VulkanContext&` - Device, physical device, function loader
- `QueueManager*` - Queue management for transfer optimization
- Buffer/image creation parameters (size, usage, properties)
- Raw data for staging operations

**Output:**
- `ResourceHandle` - RAII wrapper with automatic cleanup
- Staging regions for CPU→GPU transfers
- Memory statistics and pressure monitoring data
- Graphics pipeline resources (UBO/vertex/index buffers)

**Data Flow:** Delegates to specialized managers → Returns unified ResourceHandle

### ResourceHandle (RAII Wrapper)
**Structure:**
```cpp
struct ResourceHandle {
    vulkan_raii::Buffer buffer;        // Auto-cleanup Vulkan buffer
    vulkan_raii::Image image;          // Auto-cleanup Vulkan image  
    vulkan_raii::ImageView imageView;  // Auto-cleanup image view
    vulkan_raii::DeviceMemory memory;  // Auto-cleanup memory
    void* mappedData = nullptr;        // CPU-mapped pointer
    VkDeviceSize size = 0;             // Allocation size
    bool isValid() const;              // Validity check
};
```
**Key:** Move-only semantics, automatic cleanup, integrated validity checking

### MemoryAllocator
**Input:**
- `VkMemoryRequirements` - Memory requirements from Vulkan objects
- `VkMemoryPropertyFlags` - Memory property requirements (device-local, host-visible, etc.)

**Output:**
- `AllocationInfo` - Memory allocation with metadata
- `DeviceMemoryBudget` - Memory pressure monitoring data
- `MemoryStats` - Allocation statistics and fragmentation tracking

**Features:** Custom VMA implementation, pressure detection, fragmentation mitigation

### BufferFactory
**Input:**
- Buffer size, usage flags, memory properties
- Image dimensions, format, usage flags
- Raw data for copying operations

**Output:**
- `ResourceHandle` for buffers/images
- Completed transfer operations
- Resource destruction coordination

**Dependencies:** Requires MemoryAllocator, StagingRingBuffer, CommandExecutor

### CommandExecutor
**Input:**
- `QueueManager*` - Queue management for transfer optimization
- Source/destination buffers for copy operations
- Transfer sizes and offsets

**Output:**
- Synchronous transfers (graphics queue)
- `AsyncTransfer` - Asynchronous transfers (dedicated transfer queue)
- Transfer completion tracking

**Queue Selection:** Dedicated transfer → Graphics fallback, automatic optimization

### StagingRingBuffer (CPU→GPU Transfer)
**Input:**
- Raw data pointers and sizes
- Alignment requirements

**Output:**
- `StagingRegion` - CPU-writable memory regions
- `StagingRegionGuard` - RAII wrapper for automatic management

**Key Structure:**
```cpp
struct StagingRegion {
    void* mappedData;           // Direct CPU write access
    VkBuffer buffer;            // Source for transfers
    VkDeviceSize offset, size;  // Ring buffer position
    bool isValid() const;
};
```

### DescriptorPoolManager
**Input:**
- `DescriptorPoolConfig` - Pool size configuration
- Descriptor set layout requirements

**Output:**
- `vulkan_raii::DescriptorPool` - RAII descriptor pools
- Pool destruction coordination

**Default Config:** 1024 max sets, uniform/storage buffers, images, samplers

### GraphicsResourceManager
**Input:**
- `BufferFactory*` - For resource creation
- Descriptor set layouts
- Entity/position buffer handles for updates

**Output:**
- Uniform buffers (per-frame UBO data)
- Triangle vertex/index buffers
- Graphics descriptor pools and sets
- Descriptor set updates for compute→graphics binding

### ResourceUtils (Static Utilities)
**Input:**
- Vulkan device/loader contexts
- Buffer/descriptor parameters
- Memory requirements

**Output:**
- Common buffer creation patterns
- Descriptor pool/set utilities
- Memory mapping/unmapping operations
- Debug name setting

## Key Data Flow Patterns

### Resource Creation:
1. `ResourceContext::createBuffer()` → `BufferFactory::createBuffer()`
2. `MemoryAllocator::allocateMemory()` → Memory allocation
3. Buffer binding + optional mapping → `ResourceHandle` returned

### Staging Transfer:
1. `StagingRingBuffer::allocateGuarded()` → CPU-writable region
2. Direct CPU write to `mappedData`
3. `CommandExecutor::copyBufferToBufferAsync()` → Optimal queue transfer
4. Async completion via QueueManager fence management

### Graphics Integration:
1. `GraphicsResourceManager::createUniformBuffers()` → Per-frame UBO
2. `createTriangleBuffers()` → Entity rendering geometry
3. `updateDescriptorSetsWithEntityAndPositionBuffers()` → Compute→Graphics binding

## External Dependencies

### Core Infrastructure:
- `vulkan/core/vulkan_context.h` - Device access and function loader
- `vulkan/core/vulkan_raii.h` - RAII wrapper types for automatic cleanup
- `vulkan/core/queue_manager.h` - Transfer queue optimization
- `vulkan/core/vulkan_constants.h` - Default pool sizes and limits

### Integration Points:
- `ecs/gpu_entity_manager.h` - Entity buffer allocation and staging
- `vulkan_renderer.h` - Master renderer resource context ownership
- `vulkan/nodes/entity_graphics_node.h` - Graphics pipeline binding
- `vulkan/rendering/frame_graph_resource_registry.h` - Frame graph resource registration

## Key Notes & Gotchas

### Memory Pressure Handling:
- Automatic detection with >80% memory usage threshold
- Recovery mechanisms include cache eviction and fragmentation mitigation
- Critical fragmentation detection (>50% wasted space)

### Queue Optimization:
- Dedicated transfer queues used when available for async operations
- Graphics queue fallback ensures compatibility
- Transfer completion tracked via QueueManager's fence system

### RAII Management:
- All Vulkan objects use vulkan_raii wrappers for automatic cleanup
- Move-only semantics prevent accidental resource duplication
- Explicit `cleanupBeforeContextDestruction()` prevents use-after-free

### Ring Buffer Edge Cases:
- Wrap-around handling with fragmentation detection
- Alignment requirements enforced for GPU structures
- Memory pressure integration triggers defragmentation

### Descriptor Pool Limits:
- Default pools sized for 1024 descriptor sets
- Pool exhaustion triggers automatic recreation
- Bindless-ready configuration available but not enabled

## Update Requirements
This documentation must be updated if:
- QueueManager interface changes (affects CommandExecutor async operations)
- vulkan_raii wrapper types change (affects ResourceHandle structure)
- Memory pressure thresholds change (affects MemoryAllocator behavior)
- Frame graph resource lifetime changes (affects integration patterns)
- Graphics pipeline descriptor layouts change (affects GraphicsResourceManager updates)
- VMA implementation changes (affects MemoryAllocator allocation patterns)