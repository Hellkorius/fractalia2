# Vulkan Monitoring Subsystem

## Purpose
GPU performance monitoring and stability validation subsystem for preventing VK_ERROR_DEVICE_LOST and ensuring compute pipeline reliability in high-load scenarios (80,000+ entities at 60 FPS).

## File/Folder Hierarchy
```
monitoring/
├── gpu_memory_monitor.h/.cpp     # Memory usage and bandwidth monitoring
├── gpu_timeout_detector.h/.cpp   # Compute dispatch timeout detection/recovery
├── compute_stress_tester.h/.cpp  # Pipeline stability validation and stress testing
└── CLAUDE.md                     # This documentation
```

## Component Inputs & Outputs

### GPUMemoryMonitor
**Purpose:** Tracks GPU memory utilization patterns and bandwidth to prevent memory-induced device timeouts.

**Inputs:**
- `VulkanContext*` - Vulkan device/physical device access for memory queries
- `VkBuffer` handles via `trackBufferAllocation()`/`recordBufferAccess()`
- Buffer access patterns (size, read/write flag) per frame

**Outputs:**
- `MemoryStats` structure containing:
  - Total/used/available device memory (uint64_t bytes)
  - Memory utilization percentage
  - Buffer-specific memory tracking (entity, position, vertex buffers)
  - Estimated vs theoretical bandwidth (GB/s)
- `MemoryRecommendation` structure with optimization suggestions
- Health indicators: `isMemoryHealthy()`, `getMemoryPressure()` (0.0-1.0 scale)

**Data Flow:**
1. `beginFrame()` → Reset frame tracking counters, start timing
2. `recordBufferAccess(buffer, size, isWrite)` → Accumulate frame bandwidth
3. `trackBufferAllocation(buffer, size, name)` → Register buffer for monitoring
4. `endFrame()` → Calculate bandwidth, update rolling averages, generate stats
5. External systems query `getStats()`/`getRecommendations()` for decisions

### GPUTimeoutDetector
**Purpose:** Monitors compute dispatch execution time with automatic recovery to prevent VK_ERROR_DEVICE_LOST.

**Inputs:**
- `VulkanContext*` - Device access for status queries
- `VulkanSync*` - Synchronization primitives for fence operations
- `TimeoutConfig` - Thresholds: warning (16ms), critical (50ms), device lost (100ms)
- Dispatch timing via `beginComputeDispatch(name, workgroupCount)` / `endComputeDispatch()` pairs

**Outputs:**
- `DispatchStats` structure containing:
  - Average/peak dispatch times (milliseconds)
  - Warning/critical count tracking
  - Throughput metrics (entities/ms)
- `RecoveryRecommendation` structure with workload reduction advice
- Auto-recovery actions: Reduces `recommendedMaxWorkgroups` by 25% on consecutive failures
- Device health status: `isGPUHealthy()`, `getLastDeviceStatus()`

**Data Flow:**
1. `beginComputeDispatch(name, workgroups)` → Start CPU timing, check device status
2. [External compute dispatch execution]
3. `endComputeDispatch()` → Stop timing, check thresholds, trigger auto-recovery if needed
4. Stats updated in rolling window (30 samples), consecutive warning tracking
5. External systems query `getRecoveryRecommendation()` for workload adjustments

### ComputeStressTester
**Purpose:** Validates compute pipeline stability under various loads to establish safe operational parameters.

**Inputs:**
- `VulkanContext*` - Vulkan device access
- `ComputePipelineManager*` - Pipeline dispatch capabilities
- `GPUTimeoutDetector*` (optional) - Integration for timeout monitoring during tests
- `GPUMemoryMonitor*` (optional) - Integration for memory pressure testing
- `StressTestConfig` - Test parameters (max workgroups, iterations, thresholds)

**Outputs:**
- `StressTestResult` structure containing:
  - Maximum stable workgroup count before failure
  - Performance metrics (average/peak dispatch time, throughput)
  - GPU utilization and memory bandwidth measurements
  - Error/warning lists with specific failure points
  - Recommendations (max workgroups, chunking, workgroup size)

**Data Flow:**
1. `createTestResources()` → Allocate test buffers (200k entities max), command pools, descriptors
2. Test execution methods:
   - `runQuickValidation(targetWorkgroups)` → Single target test (5 iterations)
   - `runProgressiveLoad(start, max)` → Incremental testing (250 workgroup steps)
   - `runStressTest(config)` → Full configurable stress testing
3. Internal dispatch loop: `executeComputeDispatch()` → Record timing, validate results
4. `addRecommendations()` → Generate operational guidance based on test results
5. `destroyTestResources()` → Clean up test infrastructure

## Peripheral Dependencies

### Core Vulkan Infrastructure
- **VulkanContext** (`../core/vulkan_context.h`) - Device, physical device, memory properties
- **VulkanSync** (`../core/vulkan_sync.h`) - Fence operations for timeout detection
- **VulkanFunctionLoader** (`../core/vulkan_function_loader.h`) - Vulkan API access

### Pipeline Management
- **ComputePipelineManager** (`../pipelines/compute_pipeline_manager.h`) - Compute dispatch execution
- **DescriptorLayoutManager** (`../pipelines/descriptor_layout_manager.h`) - Descriptor set management for stress testing

### Integration Points
- **EntityComputeNode** (`../nodes/entity_compute_node.h`) - Primary consumer of monitoring services
  - Uses `GPUTimeoutDetector` for adaptive workgroup limiting
  - Integrates timeout detection in compute dispatch loops

## Key Notes

### Operational Patterns
- **Frame-based Monitoring:** Memory and timeout detection operate on frame boundaries matching 60 FPS target
- **Rolling Averages:** 60-sample bandwidth history, 30-sample dispatch time windows for smooth metrics
- **Auto-recovery:** Timeout detector automatically reduces workgroups by 25% on consecutive failures (≥3 warnings)
- **Conservative Estimates:** Bandwidth calculations use conservative GPU-specific estimates to prevent false positives

### Critical Thresholds
- **Memory Pressure:** >85% utilization triggers warnings, >95% critical
- **Dispatch Timeouts:** 16ms warning, 50ms critical, 100ms device-lost threshold
- **Stress Testing:** 200k entity maximum for validation, 50ms timeout limit per dispatch

### Integration Requirements
- Monitoring components designed for optional integration (nullptr checks throughout)
- Thread-safe for frame-based access patterns
- CPU-based timing fallback when GPU timestamp queries unavailable
- Error handling designed to prevent monitoring failures from affecting rendering

### Performance Impact
- Minimal overhead: O(1) per frame for memory monitoring
- O(1) per dispatch for timeout detection
- Stress testing is offline/development-time only (high resource usage during testing)

---
**Note:** This documentation must be updated if any referenced core components, pipeline managers, or integration points change their APIs or data structures.