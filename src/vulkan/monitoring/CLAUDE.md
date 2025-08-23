# /src/vulkan/monitoring

**monitoring/** (GPU performance monitoring and stability validation tools to prevent VK_ERROR_DEVICE_LOST)

## Files

### compute_stress_tester.h
**Inputs:** VulkanContext, ComputePipelineManager, optional GPUTimeoutDetector and GPUMemoryMonitor instances.
**Outputs:** StressTestResult structures with stability metrics, performance recommendations, and safe workgroup limits.
Validates compute pipeline stability under various workloads to prevent device crashes during production.

### compute_stress_tester.cpp
**Inputs:** Test configurations, workgroup counts, timeout thresholds, and GPU pipeline state.
**Outputs:** Executed compute dispatches with timing data, device status validation, and progressive load test results.
Implements comprehensive compute stress testing with RAII resource management and auto-recovery mechanisms.

### gpu_memory_monitor.h
**Inputs:** VulkanContext, buffer allocation/deallocation events, and memory access patterns.
**Outputs:** MemoryStats with utilization percentages, bandwidth estimates, and performance recommendations.
Tracks GPU memory usage and bandwidth to detect bottlenecks that could cause timeouts.

### gpu_memory_monitor.cpp
**Inputs:** VkPhysicalDeviceMemoryProperties, buffer access events, and GPU vendor information.
**Outputs:** Memory pressure metrics, bandwidth utilization calculations, and optimization recommendations.
Implements frame-based memory monitoring with rolling averages and vendor-specific bandwidth estimation.

### gpu_timeout_detector.h
**Inputs:** VulkanContext, VulkanSync, compute dispatch timing events, and timeout configuration.
**Outputs:** RecoveryRecommendation structures with workload adjustments and DispatchStats with performance metrics.
Monitors compute dispatch execution time for early VK_ERROR_DEVICE_LOST prevention.

### gpu_timeout_detector.cpp
**Inputs:** Compute dispatch begin/end events, GPU timestamp queries, and device status polling.
**Outputs:** Timeout warnings, auto-recovery workgroup reductions, and moving average performance statistics.
Implements precise timing with GPU timestamps and automatic workload adjustment to maintain GPU stability.