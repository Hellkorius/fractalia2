#pragma once

#include <vulkan/vulkan.h>

// Frame synchronization constants
inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// Fence timeout constants for smooth 60fps
constexpr uint64_t FENCE_TIMEOUT_IMMEDIATE = 0;           // Immediate check
constexpr uint64_t FENCE_TIMEOUT_FRAME = 16000000;       // 16ms (one frame at 60fps)

// GPU entity constants
constexpr uint32_t GPU_ENTITY_SIZE = 128; // 128 bytes per GPUEntity