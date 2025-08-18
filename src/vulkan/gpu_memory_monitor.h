#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <string>

// Forward declarations
class VulkanContext;

/**
 * GPU Memory Bandwidth Monitor - Tracks memory usage patterns and bandwidth utilization
 * to detect potential bottlenecks that could cause device timeouts
 */
class GPUMemoryMonitor {
public:
    GPUMemoryMonitor(const VulkanContext* context);
    ~GPUMemoryMonitor() = default;

    // Memory statistics
    struct MemoryStats {
        uint64_t totalDeviceMemory = 0;           // Total GPU memory
        uint64_t usedDeviceMemory = 0;            // Currently allocated
        uint64_t availableDeviceMemory = 0;       // Available for allocation
        float memoryUtilizationPercent = 0.0f;   // Used / Total * 100
        
        // Buffer-specific tracking
        uint64_t entityBufferSize = 0;
        uint64_t positionBufferSize = 0;
        uint64_t vertexBufferSize = 0;
        uint64_t totalBufferMemory = 0;
        
        // Bandwidth estimation
        float estimatedBandwidthGBps = 0.0f;      // Based on buffer access patterns
        float theoreticalBandwidthGBps = 0.0f;   // GPU specification limit
        float bandwidthUtilizationPercent = 0.0f; // Estimated / Theoretical * 100
    };
    
    // Monitoring interface
    void beginFrame();
    void endFrame();
    void recordBufferAccess(VkBuffer buffer, uint64_t accessSize, bool isWrite);
    
    // Memory allocation tracking
    void trackBufferAllocation(VkBuffer buffer, uint64_t size, const char* name);
    void trackBufferDeallocation(VkBuffer buffer);
    
    // Statistics and health
    MemoryStats getStats() const { return currentStats; }
    bool isMemoryHealthy() const;
    float getMemoryPressure() const; // 0.0 = no pressure, 1.0 = critical
    
    // Performance recommendations
    struct MemoryRecommendation {
        bool shouldReduceBufferSizes = false;
        bool shouldOptimizeAccessPatterns = false;
        bool shouldEnableCompression = false;
        float recommendedMemoryLimit = 0.0f; // In GB
        
        // Specific recommendations
        std::vector<std::string> recommendations;
    };
    
    MemoryRecommendation getRecommendations() const;

private:
    const VulkanContext* context;
    MemoryStats currentStats{};
    
    // Frame-based tracking
    std::chrono::high_resolution_clock::time_point frameStartTime;
    uint64_t frameBufferAccessBytes = 0;
    uint32_t frameAccessCount = 0;
    
    // Buffer tracking
    struct BufferInfo {
        uint64_t size;
        std::string name;
        uint64_t totalAccesses = 0;
        uint64_t bytesAccessed = 0;
        std::chrono::high_resolution_clock::time_point lastAccess;
    };
    
    std::unordered_map<VkBuffer, BufferInfo> trackedBuffers;
    
    // Performance history
    std::vector<float> recentBandwidthSamples;
    static constexpr size_t BANDWIDTH_HISTORY_SIZE = 60; // 1 second at 60 FPS
    
    // Internal methods
    void updateMemoryStats();
    void calculateBandwidth();
    float getTheoreticalBandwidth() const;
    bool queryDeviceMemoryInfo();
};