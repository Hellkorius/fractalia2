#pragma once

#include <atomic>
#include <cstdint>

// Frame Graph Debug Configuration
// Enable debug counters only in debug builds or when explicitly requested
#if defined(DEBUG) || defined(_DEBUG) || defined(FRAME_GRAPH_DEBUG_COUNTERS)
    #define FRAME_GRAPH_DEBUG_ENABLED 1
#else
    #define FRAME_GRAPH_DEBUG_ENABLED 0
#endif

namespace FrameGraphDebug {

// Conditional debug counter - zero overhead in release builds
#if FRAME_GRAPH_DEBUG_ENABLED
    using DebugCounter = std::atomic<uint32_t>;
    
    // Debug counter increment with return value for conditional logging
    inline uint32_t incrementCounter(DebugCounter& counter) {
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Debug counter value access
    inline uint32_t getCounterValue(const DebugCounter& counter) {
        return counter.load(std::memory_order_relaxed);
    }
    
    // Debug counter reset
    inline void resetCounter(DebugCounter& counter) {
        counter.store(0, std::memory_order_relaxed);
    }
#else
    // No-op debug counter for release builds
    struct DebugCounter {
        constexpr DebugCounter() = default;
        constexpr DebugCounter(uint32_t) {}
    };
    
    // All debug operations are no-ops in release builds
    constexpr uint32_t incrementCounter(DebugCounter&) { return 0; }
    constexpr uint32_t getCounterValue(const DebugCounter&) { return 0; }
    constexpr void resetCounter(DebugCounter&) {}
#endif

// Debug logging macros with counter-based throttling
#define FRAME_GRAPH_DEBUG_LOG_THROTTLED(counter, interval, message) \
    do { \
        if constexpr (FRAME_GRAPH_DEBUG_ENABLED) { \
            uint32_t count = FrameGraphDebug::incrementCounter(counter); \
            if (count % (interval) == 0) { \
                std::cout << "[FrameGraph Debug] " << message << " (occurrence #" << count << ")" << std::endl; \
            } \
        } \
    } while(0)

// Simple debug logging (only in debug builds)
#define FRAME_GRAPH_DEBUG_LOG(message) \
    do { \
        if constexpr (FRAME_GRAPH_DEBUG_ENABLED) { \
            std::cout << "[FrameGraph Debug] " << message << std::endl; \
        } \
    } while(0)

// Utility functions for common debug patterns
#if FRAME_GRAPH_DEBUG_ENABLED
    #include <unordered_map>
    #include <string>
    
    // Consolidated node execution logging - eliminates duplicate debug code in all compute nodes
    inline void logNodeExecution(const std::string& nodeName, uint32_t entityCount, uint32_t workgroups, uint32_t throttleInterval = 1800) {
        static std::unordered_map<std::string, DebugCounter> nodeCounters;
        auto& counter = nodeCounters[nodeName];
        FRAME_GRAPH_DEBUG_LOG_THROTTLED(counter, throttleInterval, 
            nodeName << ": " << entityCount << " entities → " << workgroups << " workgroups");
    }
    
    // Consolidated chunked execution logging
    inline void logChunkedExecution(const std::string& nodeName, uint32_t chunkCount, uint32_t maxWorkgroupsPerChunk, uint32_t entityCount) {
        static std::unordered_map<std::string, DebugCounter> chunkCounters;
        auto& counter = chunkCounters[nodeName + "_chunks"];
        uint32_t logCount = incrementCounter(counter);
        if (logCount % 300 == 0) {
            std::cout << "[FrameGraph Debug] " << nodeName << ": Split dispatch into " << chunkCount 
                      << " chunks (" << maxWorkgroupsPerChunk << " max) for " << entityCount 
                      << " entities (occurrence #" << logCount << ")" << std::endl;
        }
    }
#else
    // No-op versions for release builds
    inline void logNodeExecution(const std::string&, uint32_t, uint32_t, uint32_t = 1800) {}
    inline void logChunkedExecution(const std::string&, uint32_t, uint32_t, uint32_t) {}
#endif

} // namespace FrameGraphDebug