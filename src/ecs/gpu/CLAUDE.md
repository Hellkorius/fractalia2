# ECS GPU Bridge - MVP Transformation Architecture

## /home/nar/projects/fractalia2/src/ecs/gpu (CPU-GPU bridge with model matrix-based positioning)

### Architecture Overview
GPU entity management with **Structure of Arrays (SoA)** layout optimized for compute shader performance. **Model matrices serve as the authoritative position source** - physics compute shaders read/write positions from/to model matrix column 3, eliminating separate position buffer dependencies.

### Buffer Management Patterns
- **SoA Specialized Buffers**: Single-responsibility buffer classes inheriting from BufferBase
- **Model Matrix Authority**: Initial positions from `Transform::getMatrix()`, physics updates column 3
- **Read-Write Model Matrices**: Physics compute shaders update positions directly in model matrix buffer
- **Deprecated Position Buffers**: Position buffer infrastructure marked deprecated but maintained for compatibility

---

### buffer_base.h
**Inputs:** VulkanContext, ResourceCoordinator, buffer configuration parameters  
**Outputs:** Abstract base class providing common buffer operations interface  
RAII buffer lifecycle management with IBufferOperations interface. Template pattern for specialized buffer implementations with shared staging operations.

### buffer_base.cpp
**Inputs:** Buffer initialization parameters, data for upload/readback operations  
**Outputs:** Vulkan buffer creation, memory allocation, staging transfer coordination  
Implements common buffer operations with ResourceCoordinator staging infrastructure. Provides copyData/readData methods for GPU synchronization.

### buffer_operations_interface.h
**Inputs:** None (interface definition)  
**Outputs:** IBufferOperations interface contract for buffer abstraction  
Abstract interface enabling template-based upload service while maintaining Single Responsibility Principle across buffer types.

### buffer_upload_service.h/.cpp
**Inputs:** ResourceCoordinator, IBufferOperations-compliant buffer objects  
**Outputs:** Template-based upload service with validation and batch processing  
Generic upload logic shared across specialized buffers. Eliminates code duplication for buffer validation and staging operations.

### entity_buffer_manager.h/.cpp
**Inputs:** VulkanContext, ResourceCoordinator, maxEntities configuration  
**Outputs:** SoA buffer coordination with model matrix-based positioning  
**Key Changes**: Manages specialized SoA buffers using composition pattern. Readback methods extract positions from model matrix column 3 (`entityId * 4 + 3`). Provides debug readback for spatial hash collision detection with model matrix position source.

### entity_buffer_types.h
**Inputs:** None (constants and enums)  
**Outputs:** Buffer type enumeration with deprecation markers  
**Deprecation Status**: `POSITION_OUTPUT` and `CURRENT_POSITION` marked as deprecated - physics now writes to `MODEL_MATRIX` column 3 instead of separate position buffers.

### entity_descriptor_bindings.h
**Inputs:** None (binding layout constants)  
**Outputs:** Centralized binding constants for compute/graphics pipelines  
Defines descriptor set bindings including `MODEL_MATRIX_BUFFER` (binding 8) for compute shaders. Graphics pipeline uses legacy position bindings for compatibility.

### entity_descriptor_manager.h/.cpp
**Inputs:** EntityBufferManager buffer handles, ResourceCoordinator uniform buffers  
**Outputs:** Descriptor set management with model matrix buffer binding  
Creates descriptor sets binding SoA entity buffers to compute/graphics pipelines. Model matrix buffer bound as storage buffer for compute shader read/write access.

### gpu_entity_manager.h/.cpp
**Inputs:** Flecs ECS entities with Transform/Renderable/MovementPattern components  
**Outputs:** GPU buffer uploads with model matrix initialization from Transform::getMatrix()  
**MVP Implementation**: 
- `GPUEntitySoA::addFromECS()` calls `transform.getMatrix()` for initial model matrix
- Physics compute shaders read/write positions from model matrix column 3
- Eliminates dependency on separate position buffer uploads
- Debug output shows model matrix position verification

### position_buffer_coordinator.h/.cpp
**Inputs:** VulkanContext, ResourceCoordinator, maxEntities configuration  
**Outputs:** **DEPRECATED** ping-pong position buffer coordination  
**Status**: Maintained for compatibility but **deprecated in MVP architecture**. Modern physics pipeline uses model matrix buffer for position read/write operations instead of ping-pong position buffers.

### specialized_buffers.h
**Inputs:** VulkanContext, ResourceCoordinator, buffer-specific configurations  
**Outputs:** SRP-compliant specialized buffer classes inheriting from BufferBase  
**Model Matrix Focus**: `ModelMatrixBuffer` class supports read-write access for physics compute shaders. Other specialized buffers (velocity, movement params, runtime state, color, spatial map) maintain single-responsibility design with BufferBase common operations.