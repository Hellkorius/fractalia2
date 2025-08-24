# Buffer Management Subsystem

**/home/nar/projects/fractalia2/src/vulkan/resources/buffers** (Manages Vulkan buffer lifecycle, memory allocation, staging operations, and transfer orchestration)

## Files

### buffer_factory.h/cpp
**Inputs:** VulkanContext, MemoryAllocator, buffer specifications (size, usage flags, memory properties), StagingBufferPool, CommandExecutor
**Outputs:** Creates ResourceHandle-wrapped Vulkan buffers, images, and image views. Executes copy operations between buffers using staging transfers.

### buffer_manager.h/cpp  
**Inputs:** ResourceCoordinator, staging buffer size configuration
**Outputs:** Provides unified facade coordinating StagingBufferPool, BufferRegistry, TransferOrchestrator, and BufferStatisticsCollector. Delegates specialized buffer operations to appropriate subsystems.

### buffer_registry.h/cpp
**Inputs:** ResourceCoordinator, BufferFactory, GPUBuffer creation requests
**Outputs:** Manages GPUBuffer lifecycle registration and tracking. Provides registry statistics and identifies buffers with pending upload operations.

### buffer_statistics_collector.h/cpp
**Inputs:** StagingBufferPool, BufferRegistry, TransferOrchestrator statistics
**Outputs:** Aggregates buffer subsystem metrics including memory pressure indicators, fragmentation ratios, and transfer performance stats. Implements StatisticsProvider interface.

### gpu_buffer.h/cpp
**Inputs:** ResourceCoordinator, BufferManager, buffer specifications (size, usage, memory properties), raw data for staging
**Outputs:** High-level buffer abstraction with automatic staging for device-local buffers. Manages pending upload state and provides GPU-accessible buffer handles.

### staging_buffer_pool.h/cpp
**Inputs:** VulkanContext, pool size configuration, allocation requests with size and alignment  
**Outputs:** Ring buffer allocator providing temporary memory regions for GPU transfers. Implements defragmentation and allocation failure recovery with detailed fragmentation metrics.

### transfer_orchestrator.h/cpp
**Inputs:** StagingBufferPool, BufferRegistry, CommandExecutor, transfer requests and batch operations
**Outputs:** Coordinates buffer-to-buffer and host-to-buffer transfers using staging buffers and async command execution. Tracks transfer statistics and optimizes batch operations.