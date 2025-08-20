# Vulkan Monitoring Subsystem

## Purpose
GPU health monitoring and performance validation to prevent VK_ERROR_DEVICE_LOST and optimize compute workloads for 80,000+ entity rendering at 60 FPS.

## File Hierarchy
```
monitoring/
├── gpu_memory_monitor.h/.cpp        # Memory bandwidth and allocation tracking  
├── gpu_timeout_detector.h/.cpp      # Compute dispatch timing and timeout prevention
├── compute_stress_tester.h/.cpp     # Compute pipeline load testing and validation
└── CLAUDE.md                        # This documentation
```

## Inputs & Outputs by Component

### GPUMemoryMonitor
**Inputs:**
- `VulkanContext*` - Physical device memory properties via `vkGetPhysicalDeviceMemoryProperties()`
- Buffer lifecycle calls - `trackBufferAllocation(VkBuffer, uint64_t size, const char* name)` and `trackBufferDeallocation(VkBuffer)`
- Frame-based access tracking - `recordBufferAccess(VkBuffer, uint64_t accessSize, bool isWrite)`
- Frame boundaries - `beginFrame()` and `endFrame()` timing markers

**Outputs:**
- `MemoryStats` struct - Total/used/available memory (uint64_t), utilization percentages, buffer-specific sizes, bandwidth estimates (GB/s)
- Health indicators - `isMemoryHealthy()` boolean, `getMemoryPressure()` float (0.0-1.0)
- `MemoryRecommendation` struct - Optimization flags and string recommendations

**Data Flow:**
1. Initialization → Query device memory heaps, calculate theoretical bandwidth
2. Runtime tracking → `beginFrame()` starts timing, `recordBufferAccess()` accumulates data
3. Frame completion → `endFrame()` calculates bandwidth, updates 60-sample rolling history
4. Statistics → `updateMemoryStats()` provides current utilization and recommendations

### GPUTimeoutDetector  
**Inputs:**
- `VulkanContext*` - Device status queries and GPU timestamp support detection
- `VulkanSync*` - Fence operations for device status checking
- Dispatch timing - `beginComputeDispatch(const char* name, uint32_t workgroupCount)` and `endComputeDispatch()`
- `TimeoutConfig` - Configurable thresholds (16ms warning, 50ms critical, 100ms device-lost)

**Outputs:**
- `DispatchStats` - Average/peak timing, throughput (entities/ms), warning/critical counts
- `RecoveryRecommendation` - Workload reduction flags, recommended max workgroups, safe timing estimates
- Device health status - `isGPUHealthy()` boolean, `getLastDeviceStatus()` VkResult
- Auto-recovery actions - Reduces `recommendedMaxWorkgroups` after consecutive warnings

**Data Flow:**
1. Initialization → Create VkQueryPool for GPU timestamps (fallback to CPU timing)
2. Dispatch monitoring → `beginComputeDispatch()` starts timing and device status check
3. Completion → `endComputeDispatch()` stops timing, updates 30-sample rolling window
4. Threshold checking → Compare against config thresholds, trigger auto-recovery
5. Status reporting → Provide recommendations and health indicators

### ComputeStressTester
**Inputs:**
- `VulkanContext*`, `ComputePipelineManager*` - Core Vulkan infrastructure for dispatch execution
- Optional monitoring - `std::shared_ptr<GPUTimeoutDetector>`, `std::shared_ptr<GPUMemoryMonitor>`
- `StressTestConfig` - Test parameters (max workgroups, increments, iterations, timeout thresholds)
- GPUEntity data layout - 128-byte entity structure for validation testing

**Outputs:**
- `StressTestResult` - Pass/fail status, max stable workgroups, performance metrics, error/warning lists
- Performance benchmarks - Throughput (entities/second), bandwidth (GB/s), utilization percentages
- Safety recommendations - Optimal workgroup counts, chunking suggestions, safe dispatch timing

**Data Flow:**
1. Resource creation → Allocate test buffers (200k entities max), command pools, descriptor sets
2. Progressive testing → Execute workload increments (250 workgroup steps) with validation
3. Result validation → Verify compute shader outputs match expected movement calculations
4. Recommendation generation → Analyze performance data to suggest production limits

## Peripheral Dependencies

### Integration Points
- **EntityComputeNode** (`../nodes/entity_compute_node.h`) - Uses GPUTimeoutDetector for adaptive workgroup limiting
- **FrameGraph** (`../rendering/frame_graph.h`) - Optional monitoring integration via setTimeoutDetector()/setMemoryMonitor()

### Core Dependencies
- **VulkanContext** (`../core/vulkan_context.h`) - Device queries, physical device properties, memory heap access
- **VulkanSync** (`../core/vulkan_sync.h`) - Fence operations for device status checking
- **VulkanRAII** (`../core/vulkan_raii.h`) - RAII wrappers for QueryPool, CommandPool, Buffer, DeviceMemory, Fence
- **ComputePipelineManager** (`../pipelines/compute_pipeline_manager.h`) - Compute dispatch execution
- **DescriptorLayoutManager** (`../pipelines/descriptor_layout_manager.h`) - Descriptor set creation

## Key Notes

### RAII Resource Management
- All components use vulkan_raii wrappers with explicit `cleanupBeforeContextDestruction()` methods
- VkCommandBuffer and VkDescriptorSet managed by parent pools (no individual RAII needed)
- Resource creation isolated to prevent production pipeline interference

### Performance Characteristics
- Memory monitoring: O(1) per frame, 60-sample bandwidth history
- Timeout detection: O(1) per dispatch, 30-sample timing history, GPU/CPU timing fallback
- Stress testing: High resource usage (development/offline use only)

### Critical Thresholds
- Timeout detection: 16ms warning → 50ms critical → 100ms device-lost
- Memory pressure: 80%+ heap utilization triggers recommendations
- Auto-recovery: 25% workgroup reduction after 3 consecutive warnings
- Stress testing: 200k entity maximum, progressive 250-workgroup increments

### Local Caching Pattern
- Uses `const auto& vk = context->getLoader(); const VkDevice device = context->getDevice();` for multiple Vulkan API calls
- Vendor-specific bandwidth estimates (NVIDIA: 500GB/s, AMD: 400GB/s, Intel: 100GB/s)

---
**Update Requirement:** This documentation must be updated if any referenced components in `../core/`, `../pipelines/`, `../nodes/`, `../rendering/`, or integration patterns change.