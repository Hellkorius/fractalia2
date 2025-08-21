# ECS GPU Buffer Management System

## Purpose
CPU-to-GPU bridge subsystem for Fractalia2's entity rendering pipeline. Manages Structure-of-Arrays (SoA) GPU buffers for 80k+ entities with specialized buffer classes following Single Responsibility Principle. Provides async compute/graphics pipeline support with ping-pong position buffers.

## File Hierarchy
```
src/ecs/gpu/
├── gpu_entity_manager.*              # Main CPU→GPU bridge coordinator
├── entity_buffer_manager.*           # Buffer coordination and lifecycle
├── position_buffer_coordinator.*     # Ping-pong position buffer logic
├── entity_descriptor_manager.*       # Vulkan descriptor set management
├── buffer_base.*                     # Common buffer operations base class
├── specialized_buffers.h             # SRP-compliant typed buffer classes  
├── buffer_operations_interface.h     # Buffer operations contract
├── buffer_upload_service.*           # Shared buffer upload service
└── entity_buffer_manager_old.cpp.txt # Legacy reference (deprecated)
```

## Component Inputs & Outputs

### GPUEntityManager (Primary Interface)
**Inputs:**
- `flecs::entity` collections from ECS systems
- ECS components: Transform, Renderable, MovementPattern
- VulkanContext, VulkanSync, ResourceContext dependencies

**Outputs:**
- SoA Vulkan buffers: velocity, movement params, runtime state, color, model matrix
- Ping-pong position buffers for compute/graphics synchronization
- VkDescriptorSet handles for compute and graphics pipelines

**Key Structure:**
```cpp
struct GPUEntitySoA {
    std::vector<glm::vec4> velocities;        // velocity.xy, damping, reserved
    std::vector<glm::vec4> movementParams;    // amplitude, frequency, phase, timeOffset
    std::vector<glm::vec4> runtimeStates;     // totalTime, initialized, stateTimer, entityState
    std::vector<glm::vec4> colors;            // RGBA color
    std::vector<glm::mat4> modelMatrices;     // transform matrices (cold data)
}
```

### EntityBufferManager (Buffer Coordination)
**Inputs:**
- uint32_t maxEntities capacity
- Typed data arrays (velocities, movement params, runtime states, colors, model matrices)
- VulkanContext and ResourceContext for Vulkan operations

**Outputs:**
- Individual specialized buffer VkBuffer handles
- Buffer size and capacity information
- Data upload coordination through BufferUploadService

### PositionBufferCoordinator (Ping-Pong Logic)
**Inputs:**
- uint32_t maxEntities capacity
- Position data (glm::vec4 arrays)
- uint32_t frameIndex for ping-pong selection

**Outputs:**
- Primary/alternate position buffers
- Current/target buffers for interpolation
- Frame-indexed compute write / graphics read buffers

### Specialized Buffer Classes
Each buffer class inherits from BufferBase:

**VelocityBuffer, MovementParamsBuffer, RuntimeStateBuffer, ColorBuffer:**
- Input: glm::vec4 arrays (maxEntities capacity)
- Output: Individual VkBuffer handles + memory management

**ModelMatrixBuffer:**
- Input: glm::mat4 arrays (maxEntities capacity) 
- Output: VkBuffer handle for transform data

**PositionBuffer:**
- Input: glm::vec4 position arrays
- Output: VkBuffer handles for compute shader position data

### EntityDescriptorManager (Vulkan Descriptors)
**Inputs:**
- VkBuffer handles from EntityBufferManager
- VkDescriptorSetLayout specifications
- Compute pipeline requirements (4 storage buffers)
- Graphics pipeline requirements (1 uniform + 2 storage buffers)

**Outputs:**
- VkDescriptorSetLayout for compute and graphics
- VkDescriptorSet bindings updated with buffer handles
- Descriptor pool management for swapchain recreation

### BufferUploadService (Shared Upload Logic)
**Inputs:**
- IBufferOperations* interfaces
- void* data pointers with size/offset
- Batch upload operations

**Outputs:**
- Validated data transfers to GPU buffers
- Upload success/failure status
- Consistent upload patterns across all buffer types

## Data Flow

### Entity Registration Flow:
1. **ECS→Staging**: `addEntitiesFromECS()` converts ECS components to GPUEntitySoA
2. **CPU Staging**: Data accumulated in SoA vectors in system memory
3. **GPU Upload**: `uploadPendingEntities()` transfers via BufferUploadService
4. **Descriptor Update**: EntityDescriptorManager binds buffers to descriptor sets

### Rendering Pipeline Flow:
1. **Compute Phase**: EntityComputeNode reads entity buffers, writes to position buffer
2. **Sync Point**: Ping-pong position buffers alternate compute write/graphics read  
3. **Graphics Phase**: EntityGraphicsNode reads position + entity buffers for rendering
4. **Frame Advance**: PositionBufferCoordinator manages buffer ping-pong state

### Buffer Access Pattern:
```cpp
// SoA buffer access
VkBuffer velocityBuf = gpuEntityManager.getVelocityBuffer();
VkBuffer movementBuf = gpuEntityManager.getMovementParamsBuffer();
VkBuffer positionBuf = gpuEntityManager.getComputeWriteBuffer(frameIndex);

// Descriptor binding
VkDescriptorSet computeDescSet = gpuEntityManager.getDescriptorManager().getComputeDescriptorSet();
```

## Peripheral Dependencies

### Direct Dependencies (Input Sources):
- **`../components/component.h`**: Transform, Renderable, MovementPattern ECS components
- **`/vulkan/core/`**: VulkanContext for device/loader access
- **`/vulkan/resources/`**: ResourceContext for memory allocation
- **`/vulkan/services/`**: VulkanSync for synchronization primitives

### Integration Points (Output Consumers):
- **`/vulkan/nodes/entity_compute_node.*`**: Consumes entity buffers for compute dispatch
- **`/vulkan/nodes/entity_graphics_node.*`**: Consumes position + entity buffers for rendering
- **`/ecs/services/rendering_service.*`**: Coordinates GPU entity upload scheduling
- **`/vulkan/services/render_frame_director.*`**: Frame graph integration
- **`/vulkan/rendering/frame_graph.*`**: Resource dependency management

### Service Integration:
- **WorldManager**: ECS world context for entity queries
- **RenderingService**: High-level render queue coordination
- **ControlService**: Entity lifecycle and GPU upload coordination

## Key Notes

### Performance Considerations:
- **SoA Layout**: Optimized for GPU vectorized access patterns
- **Ping-Pong Buffers**: Eliminates GPU stalls in compute→graphics pipeline
- **Batch Upload**: BufferUploadService minimizes GPU transfer overhead
- **Max Entities**: 131,072 (128k) entity hard limit for buffer sizing

### Memory Layout:
```cpp
// Cache-optimized GPUEntity mapping:
// Hot data (frequently accessed): velocity, movementParams, runtimeState, color (64 bytes)
// Cold data (infrequently accessed): modelMatrix (64 bytes)
// Total: 128 bytes per entity (2 cache lines)
```

### Vulkan Integration Patterns:
- **RAII Resource Management**: All buffers use automatic cleanup
- **Descriptor Set Recreation**: Critical for swapchain resize support
- **Function Loader Caching**: Local `vk` references in multi-call functions
- **Storage Buffer Usage**: All entity data as storage buffers (not uniform)

### Migration Notes:
- **Legacy Support**: GPUEntity struct maintained for backward compatibility
- **SRP Compliance**: Monolithic buffers split into specialized classes
- **Error Handling**: Buffer initialization failures propagate upward
- **Thread Safety**: Not thread-safe; requires external synchronization

### Edge Cases:
- **Empty Entities**: System handles zero-entity case gracefully
- **Buffer Overflow**: maxEntities enforced at upload time
- **Descriptor Invalidation**: Automatic recreation on context loss
- **Memory Pressure**: No dynamic resizing; fixed allocation at initialization

---

**Important**: This file must be updated whenever:
- ECS component definitions change (Transform, Renderable, MovementPattern)
- Vulkan pipeline shader inputs/outputs modify buffer layouts
- Frame graph resource dependencies are altered
- Service integration patterns change in rendering/control services