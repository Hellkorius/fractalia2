# Vulkan Monitoring Subsystem

## Purpose
GPU performance monitoring and stability validation subsystem for preventing VK_ERROR_DEVICE_LOST and ensuring compute pipeline reliability in high-load scenarios (80,000+ entities at 60 FPS).

## Architecture

### GPUMemoryMonitor
**Purpose:** Frame-based memory usage and bandwidth monitoring to prevent memory-induced timeouts.

**Inputs:**
- `VulkanContext*` - Physical device memory properties and heap information
- Per-frame buffer access via `recordBufferAccess(VkBuffer, size, isWrite)`
- Buffer lifecycle via `trackBufferAllocation(VkBuffer, size, name)` / `trackBufferDeallocation(VkBuffer)`

**Outputs:**
- `MemoryStats` structure:
  ```cpp
  uint64_t totalDeviceMemory;
  uint64_t usedDeviceMemory; 
  uint64_t availableDeviceMemory;
  float memoryUtilizationPercent;
  uint64_t entityBufferSize, positionBufferSize, vertexBufferSize;
  float estimatedBandwidthGBps;
  float theoreticalBandwidthGBps; // Vendor-specific conservative estimates
  float bandwidthUtilizationPercent;
  ```
- `MemoryRecommendation` with optimization flags and recommendation strings
- Health queries: `isMemoryHealthy()`, `getMemoryPressure()` (0.0-1.0)

**Data Flow:**
1. `beginFrame()` → Reset counters, start frame timing
2. `recordBufferAccess()` → Accumulate frame bandwidth statistics  
3. `endFrame()` → Calculate bandwidth from frame duration, update 60-sample rolling history
4. `updateMemoryStats()` → Query device memory status, calculate utilization

### GPUTimeoutDetector
**Purpose:** CPU/GPU timing-based compute dispatch monitoring with automatic workload reduction.

**Inputs:**
- `VulkanContext*` - Device status queries and timestamp support detection
- `VulkanSync*` - Fence operations for device status checking
- `TimeoutConfig` with thresholds (default: 16ms warning, 50ms critical, 100ms device-lost)
- Dispatch lifecycle via `beginComputeDispatch(name, workgroups)` / `endComputeDispatch()`

**Outputs:**
- `DispatchStats` structure:
  ```cpp
  float averageDispatchTimeMs, peakDispatchTimeMs;
  uint32_t totalDispatches, warningCount, criticalCount;
  float throughputEntitiesPerMs;
  ```
- `RecoveryRecommendation`:
  ```cpp
  bool shouldReduceWorkload, shouldSplitDispatches;
  uint32_t recommendedMaxWorkgroups;
  float estimatedSafeDispatchTimeMs;
  ```
- Auto-recovery: Reduces `recommendedMaxWorkgroups` by 25% after 3+ consecutive warnings
- Device health: `isGPUHealthy()`, `getLastDeviceStatus()`

**Data Flow:**
1. `beginComputeDispatch()` → Start CPU timing, check device status
2. [External dispatch execution]
3. `endComputeDispatch()` → Stop timing, update 30-sample rolling window, check thresholds
4. Auto-recovery triggers workload reduction on consecutive warnings
5. `checkDeviceStatus()` → Query device for VK_ERROR_DEVICE_LOST

**Implementation Details:**
- RAII timestamp query pool with fallback to CPU timing
- GPU timestamp queries when `timestampComputeAndGraphics == VK_TRUE`
- Vendor-specific theoretical bandwidth estimates (NVIDIA: 500 GB/s, AMD: 400 GB/s, Intel: 100 GB/s)

### ComputeStressTester
**Purpose:** Offline stability validation to determine safe operational parameters.

**Inputs:**
- `VulkanContext*`, `ComputePipelineManager*` - Core Vulkan infrastructure
- Optional `GPUTimeoutDetector`, `GPUMemoryMonitor` - Integration for test monitoring
- `StressTestConfig`:
  ```cpp
  uint32_t maxWorkgroups = 5000;
  uint32_t workgroupIncrement = 250;
  uint32_t iterationsPerSize = 10;
  float timeoutThresholdMs = 50.0f;
  bool enableMemoryPressure, enableConcurrentTests, validateResults;
  ```

**Outputs:**
- `StressTestResult`:
  ```cpp
  bool passed;
  uint32_t maxStableWorkgroups;
  float averageDispatchTimeMs, peakDispatchTimeMs;
  float throughputEntitiesPerSecond, memoryBandwidthGBps;
  struct recommendations {
      uint32_t recommendedMaxWorkgroups;
      bool shouldEnableChunking, shouldReduceWorkgroupSize;
      float safeDispatchTimeMs;
  };
  std::vector<std::string> errors, warnings;
  ```

**Test Methods:**
- `runQuickValidation(workgroups)` → Single target validation (5 iterations)
- `runProgressiveLoad(start, max)` → Incremental testing with 250-workgroup steps
- `runStressTest(config)` → Full configurable stress testing
- Specialized: `testEntityMovementDispatch()`, `testMemoryBandwidth()`, `testConcurrentDispatches()`

**Infrastructure:**
- RAII test resources: command pools, buffers (200k entities max), descriptor sets
- Test entity/position buffer allocation with random movement data
- GPU memory pressure simulation during testing

## Integration Points

### Primary Consumer
- **EntityComputeNode** (`../nodes/entity_compute_node.cpp`):
  - Integrates `std::shared_ptr<GPUTimeoutDetector>` for adaptive workgroup limiting
  - Uses timeout detection for chunked dispatch execution when workgroup count exceeds limits
  - Implements dispatch parameter calculation based on timeout detector recommendations

### Core Dependencies
- **VulkanContext** (`../core/vulkan_context.h`) - Device queries, physical device properties, memory heap access
- **VulkanSync** (`../core/vulkan_sync.h`) - Fence operations for device status checking
- **VulkanFunctionLoader** (`../core/vulkan_function_loader.h`) - Direct Vulkan API access
- **VulkanRAII** (`../core/vulkan_raii.h`) - RAII wrappers for query pools, buffers, command pools, fences
- **ComputePipelineManager** (`../pipelines/compute_pipeline_manager.h`) - Compute dispatch execution for stress testing
- **DescriptorLayoutManager** (`../pipelines/descriptor_layout_manager.h`) - Descriptor set creation for test infrastructure

## Implementation Notes

### RAII Resource Management
- All monitoring components use vulkan_raii wrappers for automatic cleanup
- Explicit `cleanupBeforeContextDestruction()` methods prevent use-after-free
- Command buffers and descriptor sets managed by parent pools (no individual RAII needed)

### Performance Characteristics
- **Memory Monitoring:** O(1) per frame, 60-sample bandwidth history rolling window
- **Timeout Detection:** O(1) per dispatch, 30-sample timing history, CPU fallback when GPU timestamps unavailable
- **Stress Testing:** High resource usage during testing (offline/development use only)

### Error Handling
- Optional integration design with nullptr checks throughout
- Monitoring failures isolated from rendering pipeline
- Conservative vendor-specific estimates prevent false device-lost detection
- Auto-recovery mechanisms reduce workload rather than failing hard

### Critical Thresholds
- **Dispatch Timeouts:** 16ms warning → 50ms critical → 100ms device-lost
- **Memory Pressure:** Device-local heap utilization monitoring
- **Stress Testing:** 200k entity buffer maximum, 50ms per-dispatch timeout limit

---
**Note:** This documentation reflects the current implementation state. Update when core dependencies or integration patterns change.