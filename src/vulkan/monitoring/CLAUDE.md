# Vulkan Monitoring Subsystem

## Purpose
GPU health monitoring and performance validation to prevent VK_ERROR_DEVICE_LOST and optimize compute workloads for 80,000+ entity rendering at 60 FPS.

## Core Architecture

### GPUTimeoutDetector
**Critical Function**: Prevents device lost errors through adaptive workload management
- **Input**: Compute dispatch timing with configurable thresholds (16ms warning, 50ms critical, 100ms device-lost)
- **Output**: Auto-recovery recommendations, health status, and workload reduction after consecutive warnings
- **Pattern**: GPU timestamp queries with CPU fallback, 30-sample rolling window for trend analysis
- **Auto-Recovery**: Reduces `recommendedMaxWorkgroups` by 25% after 3 consecutive warnings

### GPUMemoryMonitor  
**Critical Function**: Memory bandwidth tracking and allocation monitoring
- **Input**: Buffer lifecycle events, frame-based access tracking, memory heap queries
- **Output**: Real-time memory stats, bandwidth utilization (GB/s), pressure indicators (0.0-1.0)
- **Pattern**: 60-sample bandwidth history, vendor-specific theoretical limits (NVIDIA: 500GB/s, AMD: 400GB/s)
- **Alerting**: Memory pressure warnings at 80%+ heap utilization

### ComputeStressTester
**Critical Function**: Production stability validation through controlled load testing
- **Input**: Progressive workgroup testing (250-increment steps), 200k entity maximum capacity
- **Output**: Max stable workgroups, performance benchmarks, safety recommendations
- **Usage**: Development/offline testing only - high resource usage isolated from production pipeline

## Data Flow Patterns

### Monitoring Integration
```cpp
// EntityComputeNode integration pattern
if (timeoutDetector) {
    timeoutDetector->beginComputeDispatch("entity_movement", workgroupCount);
    // ... dispatch execution
    timeoutDetector->endComputeDispatch();
    auto recommendation = timeoutDetector->getRecoveryRecommendation();
    if (recommendation.shouldReduceWorkload) {
        workgroupCount = recommendation.recommendedMaxWorkgroups;
    }
}
```

### Frame-Based Monitoring
```cpp
// Per-frame monitoring cycle
memoryMonitor->beginFrame();
// ... buffer operations with recordBufferAccess() calls
memoryMonitor->endFrame();  // Calculates bandwidth, updates 60-sample history
```

## Key Technical Details

### RAII Resource Management
- Uses vulkan_raii wrappers with explicit `cleanupBeforeContextDestruction()` methods
- VkCommandBuffer and VkDescriptorSet managed by parent pools (no individual RAII needed)
- Resource creation isolated to prevent production pipeline interference

### Performance Characteristics
- **Memory monitoring**: O(1) per frame, minimal overhead with rolling bandwidth history
- **Timeout detection**: O(1) per dispatch, GPU/CPU timing fallback ensures compatibility
- **Stress testing**: High resource usage for development/validation only

### Critical Thresholds
- **Timeout**: 16ms warning → 50ms critical → 100ms device-lost
- **Memory**: 80%+ heap utilization triggers optimization recommendations
- **Recovery**: 25% workgroup reduction after 3 consecutive timeout warnings

### Integration Points
- **EntityComputeNode**: Uses GPUTimeoutDetector for adaptive workgroup limiting
- **FrameGraph**: Optional monitoring integration via setTimeoutDetector()/setMemoryMonitor()
- **Service Layer**: Isolated testing infrastructure separate from production render pipeline

## Dependencies
- **Core**: VulkanContext (device queries), VulkanSync (fence operations), vulkan_raii (resource management)
- **Pipelines**: ComputePipelineManager (dispatch execution), DescriptorLayoutManager (descriptor sets)
- **Local Caching**: Uses `const auto& vk = context->getLoader(); const VkDevice device = context->getDevice();` pattern

---
**Critical Note**: This monitoring subsystem is essential for preventing GPU driver timeouts and device lost errors that can crash the application when rendering 80,000+ entities. The timeout detector's auto-recovery mechanism is particularly crucial for maintaining stable frame rates under varying GPU loads.