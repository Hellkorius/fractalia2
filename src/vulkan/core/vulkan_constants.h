#pragma once

#include <vulkan/vulkan.h>

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

constexpr uint64_t FENCE_TIMEOUT_IMMEDIATE = 0;
constexpr uint64_t FENCE_TIMEOUT_FRAME = 16000000;

constexpr uint32_t GPU_ENTITY_SIZE = 128;