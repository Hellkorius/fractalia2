#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <chrono>
#include <memory>

// Forward declarations
class VulkanContext;
class VulkanSync;

/**
 * GPU Timeout Detector - Monitors compute dispatch execution time
 * and provides early warning/recovery for potential VK_ERROR_DEVICE_LOST
 */
class GPUTimeoutDetector {
public:
    GPUTimeoutDetector(const VulkanContext* context, VulkanSync* sync);
    ~GPUTimeoutDetector();

    // Configuration
    struct TimeoutConfig {
        float warningThresholdMs = 16.0f;    // Warn if dispatch takes >16ms
        float criticalThresholdMs = 50.0f;   // Critical if dispatch takes >50ms
        float deviceLostThresholdMs = 100.0f; // Likely device lost if >100ms
        uint32_t maxConsecutiveWarnings = 3; // Auto-reduce workload after N warnings
        bool enableAutoRecovery = true;      // Automatically reduce dispatch size
    };
    
    void configure(const TimeoutConfig& config) { this->config = config; }
    
    // Monitoring interface
    void beginComputeDispatch(const char* dispatchName, uint32_t workgroupCount);
    void endComputeDispatch();
    
    // Recovery recommendations
    struct RecoveryRecommendation {
        bool shouldReduceWorkload = false;
        uint32_t recommendedMaxWorkgroups = 0;
        bool shouldSplitDispatches = false;
        float estimatedSafeDispatchTimeMs = 0.0f;
    };
    
    RecoveryRecommendation getRecoveryRecommendation() const;
    
    // Statistics
    struct DispatchStats {
        float averageDispatchTimeMs = 0.0f;
        float peakDispatchTimeMs = 0.0f;
        uint32_t totalDispatches = 0;
        uint32_t warningCount = 0;
        uint32_t criticalCount = 0;
        float throughputEntitiesPerMs = 0.0f;
    };
    
    DispatchStats getStats() const { return stats; }
    void resetStats();
    
    // GPU health monitoring
    bool isGPUHealthy() const;
    VkResult getLastDeviceStatus() const { return lastDeviceStatus; }

private:
    const VulkanContext* context;
    VulkanSync* sync;
    TimeoutConfig config{};
    
    // Timing infrastructure
    std::chrono::high_resolution_clock::time_point dispatchStartTime;
    bool dispatchInProgress = false;
    
    // GPU timestamp queries for precise measurement
    VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
    static constexpr uint32_t MAX_TIMESTAMP_QUERIES = 64;
    uint32_t currentQueryIndex = 0;
    
    // Statistics tracking
    DispatchStats stats{};
    std::vector<float> recentDispatchTimes; // Rolling window of recent times
    static constexpr size_t ROLLING_WINDOW_SIZE = 30;
    
    // Recovery state
    uint32_t consecutiveWarnings = 0;
    uint32_t recommendedMaxWorkgroups = UINT32_MAX;
    VkResult lastDeviceStatus = VK_SUCCESS;
    
    // Internal methods
    bool createTimestampQueryPool();
    void updateStats(float dispatchTimeMs, uint32_t workgroupCount);
    float calculateMovingAverage() const;
    void checkDeviceStatus();
};