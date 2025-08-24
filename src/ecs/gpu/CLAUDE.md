# ECS GPU Bridge

## /home/nar/projects/fractalia2/src/ecs/gpu (Bridges ECS components to GPU buffers for compute/graphics pipeline)

### buffer_base.h
**Inputs:** VulkanContext, ResourceCoordinator, buffer configuration parameters  
**Outputs:** Abstract base class providing common buffer operations interface  
Defines shared buffer lifecycle and data transfer operations for specialized GPU buffer implementations.

### buffer_base.cpp
**Inputs:** Buffer initialization parameters, data for upload/readback operations  
**Outputs:** Vulkan buffer creation, memory allocation, and data transfer operations  
Implements common buffer operations using ResourceCoordinator's staging infrastructure and RAII resource management.

### buffer_operations_interface.h
**Inputs:** None (interface definition)  
**Outputs:** IBufferOperations interface contract  
Defines abstract interface for buffer operations to avoid code duplication while maintaining Single Responsibility Principle.

### buffer_upload_service.h
**Inputs:** ResourceCoordinator, buffer objects implementing IBufferOperations  
**Outputs:** Template-based upload service interface  
Provides shared service for consistent buffer upload operations with validation and batch processing capabilities.

### buffer_upload_service.cpp
**Inputs:** Upload operations, buffer validation parameters  
**Outputs:** Validated buffer uploads, batch operation results  
Implements generic buffer upload logic with validation and error handling for any IBufferOperations-compliant buffer.

### entity_buffer_manager.h
**Inputs:** VulkanContext, ResourceCoordinator, maxEntities configuration  
**Outputs:** Coordinated access to specialized entity buffer components  
Coordinates specialized SoA (Structure of Arrays) buffers for entity rendering using composition pattern.

### entity_buffer_manager.cpp
**Inputs:** Entity data for upload, spatial map queries, debug readback requests  
**Outputs:** GPU buffer uploads, spatial hash debug information, entity position data  
Manages specialized entity buffers and provides debug readback capabilities for spatial hash collision detection.

### entity_descriptor_bindings.h
**Inputs:** None (constants definition)  
**Outputs:** Binding layout constants for compute/graphics pipelines  
Defines centralized binding constants for entity descriptor sets to eliminate magic numbers across compute and graphics shaders.

### entity_descriptor_manager.h
**Inputs:** EntityBufferManager, ResourceCoordinator, VulkanContext  
**Outputs:** Vulkan descriptor set layouts and management interface  
Manages entity-specific descriptor sets for compute and graphics pipelines using composition with DescriptorSetManagerBase.

### entity_descriptor_manager.cpp
**Inputs:** Buffer handles from EntityBufferManager, uniform buffers from ResourceCoordinator  
**Outputs:** Configured descriptor sets, descriptor pool management, swapchain recreation support  
Creates and updates descriptor sets binding SoA entity buffers to compute shaders and graphics pipeline.

### gpu_entity_manager.h
**Inputs:** Flecs ECS entities, VulkanContext, VulkanSync, ResourceCoordinator  
**Outputs:** GPU-accessible entity data, buffer handles for frame graph  
High-level manager coordinating EntityBufferManager and EntityDescriptorManager for ECS-to-GPU bridge functionality.

### gpu_entity_manager.cpp
**Inputs:** ECS entities with Transform/Renderable/MovementPattern components  
**Outputs:** GPU buffer uploads, SoA staging data conversion, entity-to-GPU index mapping  
Converts ECS component data to GPU SoA format and manages entity upload lifecycle with debug mapping.

### position_buffer_coordinator.h
**Inputs:** VulkanContext, ResourceCoordinator, maxEntities configuration  
**Outputs:** Ping-pong position buffer coordination interface  
Manages alternating read/write position buffers for async compute/graphics pipeline synchronization.

### position_buffer_coordinator.cpp
**Inputs:** Frame indices, position data for upload  
**Outputs:** Frame-synchronized buffer handles, data upload to multiple position buffers  
Implements ping-pong buffer logic ensuring graphics reads previous frame's compute output while compute writes to alternate buffer.

### specialized_buffers.h
**Inputs:** VulkanContext, ResourceCoordinator, buffer-specific configurations  
**Outputs:** Specialized buffer classes inheriting from BufferBase  
Provides SRP-compliant buffer classes for velocity, movement parameters, runtime state, color, model matrices, positions, and spatial map data.