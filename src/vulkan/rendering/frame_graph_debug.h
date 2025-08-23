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

} // namespace FrameGraphDebug