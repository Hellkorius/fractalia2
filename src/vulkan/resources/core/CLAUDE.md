# CLAUDE.md

## /home/nar/projects/fractalia2/src/vulkan/resources/core (Core resource management infrastructure with centralized utilities and coordination)

### Files

**buffer_operation_utils.h**
**Inputs:** ResourceHandle objects, CommandExecutor, buffer copy parameters, memory property flags
**Outputs:** Validated buffer copy operations, memory property checks, direct memory copies for host-visible buffers

**buffer_operation_utils.cpp**
**Inputs:** ResourceHandle pairs for copy operations, validation contexts, memory copy data
**Outputs:** Executed buffer copies via CommandExecutor, validation results, direct memcpy operations to mapped buffers

**command_executor.h**
**Inputs:** VulkanContext, QueueManager, buffer handles, copy parameters
**Outputs:** Synchronous buffer copies, asynchronous transfers with optimal queue selection, transfer completion status

**command_executor.cpp**
**Inputs:** VulkanContext initialization, buffer copy requests, queue selection criteria
**Outputs:** Command buffer recording and submission, graphics/transfer queue utilization, async transfer management via QueueManager

**memory_allocator.h**
**Inputs:** VulkanContext, memory requirements, property flags, ResourceHandle references
**Outputs:** VkDeviceMemory allocations, memory mapping operations, pressure detection metrics, allocation statistics

**memory_allocator.cpp**
**Inputs:** Memory allocation requests, mapping requirements, pressure thresholds
**Outputs:** Allocated and mapped device memory, memory pressure recovery attempts, comprehensive allocation statistics

**resource_coordinator.h**
**Inputs:** VulkanContext, QueueManager, resource creation parameters, transfer requests
**Outputs:** Coordinated resource operations via specialized managers, unified resource management interface, memory optimization

**resource_coordinator.cpp**
**Inputs:** Manager initialization dependencies, resource creation delegates, cleanup ordering
**Outputs:** Initialized manager hierarchy, delegated resource operations, coordinated cleanup and memory recovery

**resource_factory.h**
**Inputs:** VulkanContext, MemoryAllocator, resource creation specifications
**Outputs:** ResourceHandle objects for buffers/images, resource destruction operations, BufferFactory access

**resource_factory.cpp**
**Inputs:** BufferFactory initialization parameters, resource creation delegates
**Outputs:** Created resources via BufferFactory delegation, proper factory lifecycle management, resource cleanup

**resource_handle.h**
**Inputs:** RAII Vulkan object wrappers, memory handles, mapping data
**Outputs:** Unified resource representation combining buffer/image with allocation, validity checking

**statistics_provider.h**
**Inputs:** Template statistics types, provider registration, update triggers
**Outputs:** Templated statistics interface, aggregated statistics collection, performance pressure indicators

**transfer_manager.h**
**Inputs:** TransferOrchestrator dependency, transfer operation requests
**Outputs:** Buffer copy operations, async transfer coordination, orchestrator delegation

**transfer_manager.cpp**
**Inputs:** Transfer requests via orchestrator delegation, validation contexts
**Outputs:** Executed transfers through TransferOrchestrator, validated transfer operations, async transfer handles

**validation_utils.h**
**Inputs:** Template dependency lists, resource handles, Vulkan results, validation contexts
**Outputs:** Dependency validation results, error logging operations, standardized validation messages

**validation_utils.cpp**
**Inputs:** Validation parameters, error contexts, dependency arrays
**Outputs:** Boolean validation results, formatted error messages to stderr, comprehensive validation logging