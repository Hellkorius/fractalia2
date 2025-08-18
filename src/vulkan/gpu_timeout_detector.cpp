#include "gpu_timeout_detector.h"
#include "vulkan_context.h"
#include "vulkan_sync.h"
#include "vulkan_function_loader.h"
#include <iostream>
#include <algorithm>
#include <numeric>

GPUTimeoutDetector::GPUTimeoutDetector(const VulkanContext* context, VulkanSync* sync)
    : context(context), sync(sync) {
    
    // Skip timestamp query pool creation for now - use CPU timing instead
    std::cout << "GPUTimeoutDetector: Using CPU-based timing (GPU timestamp queries not available)" << std::endl;
    
    recentDispatchTimes.reserve(ROLLING_WINDOW_SIZE);
}

GPUTimeoutDetector::~GPUTimeoutDetector() {
    // No timestamp query pool to destroy - using CPU timing
}

bool GPUTimeoutDetector::createTimestampQueryPool() {
    // Not implemented - using CPU timing instead
    return true;
}

void GPUTimeoutDetector::beginComputeDispatch(const char* dispatchName, uint32_t workgroupCount) {
    if (dispatchInProgress) {
        std::cerr << "GPUTimeoutDetector: Warning - overlapping dispatch monitoring" << std::endl;
        return;
    }
    
    dispatchInProgress = true;
    dispatchStartTime = std::chrono::high_resolution_clock::now();
    
    // Check device status before dispatch
    checkDeviceStatus();
    
    if (lastDeviceStatus != VK_SUCCESS) {
        std::cerr << "GPUTimeoutDetector: Device status error before dispatch '" 
                  << dispatchName << "': " << lastDeviceStatus << std::endl;
    }
}

void GPUTimeoutDetector::endComputeDispatch() {
    if (!dispatchInProgress) {
        std::cerr << "GPUTimeoutDetector: Warning - endComputeDispatch without begin" << std::endl;
        return;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - dispatchStartTime);
    float dispatchTimeMs = durationUs.count() / 1000.0f;
    
    dispatchInProgress = false;
    
    // Check device status after dispatch
    checkDeviceStatus();
    
    // Update statistics
    updateStats(dispatchTimeMs, 0); // workgroupCount passed separately if needed
    
    // Check thresholds and provide warnings
    if (dispatchTimeMs > config.deviceLostThresholdMs) {
        std::cerr << "GPUTimeoutDetector: CRITICAL - Dispatch time " << dispatchTimeMs 
                  << "ms exceeds device lost threshold (" << config.deviceLostThresholdMs << "ms)" << std::endl;
        consecutiveWarnings = config.maxConsecutiveWarnings; // Force recovery
    } else if (dispatchTimeMs > config.criticalThresholdMs) {
        std::cerr << "GPUTimeoutDetector: CRITICAL - Dispatch time " << dispatchTimeMs 
                  << "ms exceeds critical threshold (" << config.criticalThresholdMs << "ms)" << std::endl;
        stats.criticalCount++;
        consecutiveWarnings++;
    } else if (dispatchTimeMs > config.warningThresholdMs) {
        std::cout << "GPUTimeoutDetector: WARNING - Dispatch time " << dispatchTimeMs 
                  << "ms exceeds warning threshold (" << config.warningThresholdMs << "ms)" << std::endl;
        stats.warningCount++;
        consecutiveWarnings++;
    } else {
        consecutiveWarnings = 0; // Reset on successful dispatch
    }
    
    // Auto-recovery if enabled
    if (config.enableAutoRecovery && consecutiveWarnings >= config.maxConsecutiveWarnings) {
        // Reduce recommended max workgroups by 25%
        recommendedMaxWorkgroups = static_cast<uint32_t>(recommendedMaxWorkgroups * 0.75f);
        recommendedMaxWorkgroups = std::max(recommendedMaxWorkgroups, 256u); // Minimum viable size
        
        std::cout << "GPUTimeoutDetector: Auto-recovery activated - reducing max workgroups to " 
                  << recommendedMaxWorkgroups << std::endl;
        consecutiveWarnings = 0; // Reset after applying recovery
    }
}

void GPUTimeoutDetector::updateStats(float dispatchTimeMs, uint32_t workgroupCount) {
    stats.totalDispatches++;
    stats.peakDispatchTimeMs = std::max(stats.peakDispatchTimeMs, dispatchTimeMs);
    
    // Update rolling average
    recentDispatchTimes.push_back(dispatchTimeMs);
    if (recentDispatchTimes.size() > ROLLING_WINDOW_SIZE) {
        recentDispatchTimes.erase(recentDispatchTimes.begin());
    }
    
    stats.averageDispatchTimeMs = calculateMovingAverage();
    
    // Calculate throughput if workgroup count is available
    if (workgroupCount > 0 && dispatchTimeMs > 0.0f) {
        uint32_t entitiesProcessed = workgroupCount * 64; // 64 threads per workgroup
        stats.throughputEntitiesPerMs = entitiesProcessed / dispatchTimeMs;
    }
}

float GPUTimeoutDetector::calculateMovingAverage() const {
    if (recentDispatchTimes.empty()) return 0.0f;
    
    float sum = std::accumulate(recentDispatchTimes.begin(), recentDispatchTimes.end(), 0.0f);
    return sum / recentDispatchTimes.size();
}

void GPUTimeoutDetector::checkDeviceStatus() {
    if (!context) return;
    
    // Check if device is still responsive
    VkResult result = context->getLoader().vkDeviceWaitIdle(context->getDevice());
    lastDeviceStatus = result;
    
    if (result == VK_ERROR_DEVICE_LOST) {
        std::cerr << "GPUTimeoutDetector: FATAL - VK_ERROR_DEVICE_LOST detected!" << std::endl;
    } else if (result != VK_SUCCESS) {
        std::cerr << "GPUTimeoutDetector: Device status error: " << result << std::endl;
    }
}

GPUTimeoutDetector::RecoveryRecommendation GPUTimeoutDetector::getRecoveryRecommendation() const {
    RecoveryRecommendation rec{};
    
    // Check if we should reduce workload
    if (consecutiveWarnings >= config.maxConsecutiveWarnings / 2 || 
        stats.averageDispatchTimeMs > config.warningThresholdMs) {
        rec.shouldReduceWorkload = true;
        rec.recommendedMaxWorkgroups = recommendedMaxWorkgroups;
    }
    
    // Check if we should split dispatches
    if (stats.peakDispatchTimeMs > config.criticalThresholdMs) {
        rec.shouldSplitDispatches = true;
    }
    
    // Estimate safe dispatch time based on recent performance
    if (stats.throughputEntitiesPerMs > 0.0f) {
        // Target 75% of warning threshold for safety margin
        float targetTimeMs = config.warningThresholdMs * 0.75f;
        rec.estimatedSafeDispatchTimeMs = targetTimeMs;
    }
    
    return rec;
}

bool GPUTimeoutDetector::isGPUHealthy() const {
    return lastDeviceStatus == VK_SUCCESS && 
           consecutiveWarnings < config.maxConsecutiveWarnings &&
           stats.averageDispatchTimeMs < config.criticalThresholdMs;
}

void GPUTimeoutDetector::resetStats() {
    stats = DispatchStats{};
    recentDispatchTimes.clear();
    consecutiveWarnings = 0;
    recommendedMaxWorkgroups = UINT32_MAX;
}