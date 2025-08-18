#include "gpu_memory_monitor.h"
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <unordered_map>

GPUMemoryMonitor::GPUMemoryMonitor(const VulkanContext* context) 
    : context(context) {
    
    if (!queryDeviceMemoryInfo()) {
        std::cerr << "GPUMemoryMonitor: Failed to query device memory info" << std::endl;
    }
    
    recentBandwidthSamples.reserve(BANDWIDTH_HISTORY_SIZE);
    
    // Initialize theoretical bandwidth based on GPU type (approximate)
    currentStats.theoreticalBandwidthGBps = getTheoreticalBandwidth();
}

bool GPUMemoryMonitor::queryDeviceMemoryInfo() {
    if (!context) return false;
    
    VkPhysicalDeviceMemoryProperties memProps;
    context->getLoader().vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProps);
    
    // Find the largest device-local heap
    uint64_t maxDeviceLocalSize = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            maxDeviceLocalSize = std::max(maxDeviceLocalSize, memProps.memoryHeaps[i].size);
        }
    }
    
    currentStats.totalDeviceMemory = maxDeviceLocalSize;
    currentStats.availableDeviceMemory = maxDeviceLocalSize; // Will be updated with actual usage
    
    return maxDeviceLocalSize > 0;
}

float GPUMemoryMonitor::getTheoreticalBandwidth() const {
    if (!context) return 100.0f; // Default fallback
    
    VkPhysicalDeviceProperties props;
    context->getLoader().vkGetPhysicalDeviceProperties(context->getPhysicalDevice(), &props);
    
    // Rough estimates based on GPU vendor and type
    // These are conservative estimates for timeout prevention
    switch (props.vendorID) {
        case 0x10DE: // NVIDIA
            return 500.0f; // Conservative estimate for modern NVIDIA GPUs
        case 0x1002: // AMD
            return 400.0f; // Conservative estimate for modern AMD GPUs
        case 0x8086: // Intel
            return 100.0f; // Conservative estimate for Intel integrated
        default:
            return 200.0f; // Safe default
    }
}

void GPUMemoryMonitor::beginFrame() {
    frameStartTime = std::chrono::high_resolution_clock::now();
    frameBufferAccessBytes = 0;
    frameAccessCount = 0;
}

void GPUMemoryMonitor::endFrame() {
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    auto frameDurationUs = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTime - frameStartTime);
    
    if (frameDurationUs.count() > 0 && frameBufferAccessBytes > 0) {
        // Calculate bandwidth for this frame
        float frameDurationSeconds = frameDurationUs.count() / 1000000.0f;
        float frameGigabytes = frameBufferAccessBytes / (1024.0f * 1024.0f * 1024.0f);
        float frameBandwidth = frameGigabytes / frameDurationSeconds;
        
        // Update rolling average
        recentBandwidthSamples.push_back(frameBandwidth);
        if (recentBandwidthSamples.size() > BANDWIDTH_HISTORY_SIZE) {
            recentBandwidthSamples.erase(recentBandwidthSamples.begin());
        }
        
        calculateBandwidth();
    }
    
    updateMemoryStats();
}

void GPUMemoryMonitor::recordBufferAccess(VkBuffer buffer, uint64_t accessSize, bool isWrite) {
    frameBufferAccessBytes += accessSize;
    frameAccessCount++;
    
    auto it = trackedBuffers.find(buffer);
    if (it != trackedBuffers.end()) {
        it->second.totalAccesses++;
        it->second.bytesAccessed += accessSize;
        it->second.lastAccess = std::chrono::high_resolution_clock::now();
    }
}

void GPUMemoryMonitor::trackBufferAllocation(VkBuffer buffer, uint64_t size, const char* name) {
    BufferInfo info{};
    info.size = size;
    info.name = name ? name : "Unknown";
    info.lastAccess = std::chrono::high_resolution_clock::now();
    
    trackedBuffers[buffer] = info;
    
    // Update total buffer memory
    currentStats.totalBufferMemory += size;
    
    // Update specific buffer sizes
    if (info.name.find("entity") != std::string::npos) {
        currentStats.entityBufferSize += size;
    } else if (info.name.find("position") != std::string::npos) {
        currentStats.positionBufferSize += size;
    } else if (info.name.find("vertex") != std::string::npos) {
        currentStats.vertexBufferSize += size;
    }
}

void GPUMemoryMonitor::trackBufferDeallocation(VkBuffer buffer) {
    auto it = trackedBuffers.find(buffer);
    if (it != trackedBuffers.end()) {
        currentStats.totalBufferMemory -= it->second.size;
        
        // Update specific buffer sizes
        if (it->second.name.find("entity") != std::string::npos) {
            currentStats.entityBufferSize -= it->second.size;
        } else if (it->second.name.find("position") != std::string::npos) {
            currentStats.positionBufferSize -= it->second.size;
        } else if (it->second.name.find("vertex") != std::string::npos) {
            currentStats.vertexBufferSize -= it->second.size;
        }
        
        trackedBuffers.erase(it);
    }
}

void GPUMemoryMonitor::updateMemoryStats() {
    // Update memory utilization
    currentStats.usedDeviceMemory = currentStats.totalBufferMemory; // Simplified tracking
    currentStats.availableDeviceMemory = currentStats.totalDeviceMemory - currentStats.usedDeviceMemory;
    
    if (currentStats.totalDeviceMemory > 0) {
        currentStats.memoryUtilizationPercent = 
            (float(currentStats.usedDeviceMemory) / float(currentStats.totalDeviceMemory)) * 100.0f;
    }
}

void GPUMemoryMonitor::calculateBandwidth() {
    if (recentBandwidthSamples.empty()) {
        currentStats.estimatedBandwidthGBps = 0.0f;
        currentStats.bandwidthUtilizationPercent = 0.0f;
        return;
    }
    
    // Calculate moving average bandwidth
    float totalBandwidth = std::accumulate(recentBandwidthSamples.begin(), recentBandwidthSamples.end(), 0.0f);
    currentStats.estimatedBandwidthGBps = totalBandwidth / recentBandwidthSamples.size();
    
    // Calculate utilization percentage
    if (currentStats.theoreticalBandwidthGBps > 0.0f) {
        currentStats.bandwidthUtilizationPercent = 
            (currentStats.estimatedBandwidthGBps / currentStats.theoreticalBandwidthGBps) * 100.0f;
    }
}

bool GPUMemoryMonitor::isMemoryHealthy() const {
    // Consider memory healthy if:
    // 1. Utilization is below 80%
    // 2. Bandwidth utilization is below 70%
    // 3. We have at least 500MB available
    
    const float maxUtilization = 80.0f;
    const float maxBandwidthUtilization = 70.0f;
    const uint64_t minAvailable = 500 * 1024 * 1024; // 500MB
    
    return currentStats.memoryUtilizationPercent < maxUtilization &&
           currentStats.bandwidthUtilizationPercent < maxBandwidthUtilization &&
           currentStats.availableDeviceMemory > minAvailable;
}

float GPUMemoryMonitor::getMemoryPressure() const {
    // Return a value from 0.0 (no pressure) to 1.0 (critical pressure)
    
    float utilizationPressure = currentStats.memoryUtilizationPercent / 100.0f;
    float bandwidthPressure = currentStats.bandwidthUtilizationPercent / 100.0f;
    
    // Weight memory utilization more heavily than bandwidth
    return std::min(1.0f, utilizationPressure * 0.7f + bandwidthPressure * 0.3f);
}

GPUMemoryMonitor::MemoryRecommendation GPUMemoryMonitor::getRecommendations() const {
    MemoryRecommendation rec{};
    
    float pressure = getMemoryPressure();
    
    if (pressure > 0.8f) {
        rec.shouldReduceBufferSizes = true;
        rec.recommendedMemoryLimit = currentStats.totalDeviceMemory * 0.7f / (1024.0f * 1024.0f * 1024.0f); // GB
        rec.recommendations.push_back("Critical: Reduce entity count or buffer sizes");
        rec.recommendations.push_back("Consider enabling GPU memory compression if available");
    } else if (pressure > 0.6f) {
        rec.shouldOptimizeAccessPatterns = true;
        rec.recommendations.push_back("Warning: Optimize memory access patterns");
        rec.recommendations.push_back("Consider reducing workgroup sizes");
    } else if (currentStats.bandwidthUtilizationPercent > 60.0f) {
        rec.shouldOptimizeAccessPatterns = true;
        rec.recommendations.push_back("Optimize buffer layout for better cache utilization");
    }
    
    if (currentStats.entityBufferSize > 50 * 1024 * 1024) { // > 50MB
        rec.recommendations.push_back("Entity buffer is large - consider LOD or culling");
    }
    
    return rec;
}