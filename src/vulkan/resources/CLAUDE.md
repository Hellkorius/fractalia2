# src/vulkan/resources

```
src/vulkan/resources/
├── buffers/ (Specialized buffer lifecycle, staging, transfer coordination, and statistics)
│   ├── buffer_factory.cpp
│   ├── buffer_factory.h - Consumes VulkanContext, MemoryAllocator, staging pool, command executor. Produces ResourceHandles for buffers/images with memory allocation and transfer operations.
│   ├── buffer_manager.cpp  
│   ├── buffer_manager.h - Consumes ResourceCoordinator and component factories. Orchestrates buffer operations through staging pool, registry, transfer orchestrator, and statistics collector delegation.
│   ├── buffer_registry.cpp
│   ├── buffer_registry.h - Consumes ResourceCoordinator and BufferFactory. Maintains registry of managed GPUBuffer instances with lifecycle tracking and statistical reporting.
│   ├── buffer_statistics_collector.cpp
│   ├── buffer_statistics_collector.h - Consumes staging pool, buffer registry, transfer orchestrator. Produces aggregated buffer statistics and memory pressure indicators.
│   ├── gpu_buffer.cpp
│   ├── gpu_buffer.h - Consumes ResourceCoordinator and BufferManager. Provides GPU buffer abstraction with staging data management and upload coordination.
│   ├── staging_buffer_pool.cpp
│   ├── staging_buffer_pool.h - Consumes VulkanContext for ring buffer allocation. Produces staging regions with fragmentation tracking and memory pressure detection.
│   ├── transfer_orchestrator.cpp
│   └── transfer_orchestrator.h - Consumes staging pool, buffer registry, command executor. Orchestrates single/batch/async buffer transfers with staging optimization.
├── core/ (Fundamental resource primitives and coordination infrastructure)
│   ├── buffer_operation_utils.cpp
│   ├── buffer_operation_utils.h - Consumes CommandExecutor and ResourceHandles. Provides centralized buffer copy operations and validation utilities to eliminate duplication.
│   ├── command_executor.cpp
│   ├── command_executor.h - Consumes VulkanContext and QueueManager. Executes synchronous/asynchronous buffer operations with optimal queue selection and transfer command management.
│   ├── memory_allocator.cpp
│   ├── memory_allocator.h - Consumes VulkanContext for VMA wrapper. Provides memory allocation, mapping, pressure detection, and budget management with statistics tracking.
│   ├── resource_coordinator.cpp
│   ├── resource_coordinator.h - Consumes VulkanContext and QueueManager. Coordinates resource creation, transfer operations, and manager access through delegation pattern.
│   ├── resource_factory.cpp
│   ├── resource_factory.h - Consumes VulkanContext and MemoryAllocator. Produces ResourceHandles for buffers, images, and image views with pure creation responsibility.
│   ├── resource_handle.h - Defines resource handle structure combining RAII Vulkan objects with memory allocation and mapping state.
│   ├── statistics_provider.h - Provides template-based statistics collection interface with performance monitoring and aggregation capabilities.
│   ├── transfer_manager.cpp
│   ├── transfer_manager.h - Consumes TransferOrchestrator for delegation. Provides pure transfer operations interface with single responsibility.
│   ├── validation_utils.cpp
│   └── validation_utils.h - Provides centralized validation utilities for dependencies, resources, operations, and error logging to eliminate duplication.
├── descriptors/ (Descriptor set lifecycle and update coordination)
│   ├── descriptor_set_manager_base.cpp
│   ├── descriptor_set_manager_base.h - Consumes VulkanContext and DescriptorPoolManager. Provides common descriptor set management patterns with template method lifecycle.
│   ├── descriptor_update_helper.cpp
│   └── descriptor_update_helper.h - Consumes VulkanContext and descriptor specifications. Provides pure descriptor set update utilities with templated binding patterns.
└── managers/ (High-level resource coordination and graphics pipeline management)
    ├── descriptor_pool_manager.cpp
    ├── descriptor_pool_manager.h - Consumes VulkanContext with pool configuration. Produces RAII descriptor pools with configurable sizing and bindless readiness.
    ├── graphics_resource_manager.cpp
    └── graphics_resource_manager.h - Consumes VulkanContext and BufferFactory. Manages graphics pipeline resources including uniform buffers, vertex/index buffers, and descriptor coordination.
```