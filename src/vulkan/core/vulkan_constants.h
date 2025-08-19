#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

constexpr uint64_t FENCE_TIMEOUT_IMMEDIATE = 0;
constexpr uint64_t FENCE_TIMEOUT_FRAME = 16000000;
constexpr uint64_t FENCE_TIMEOUT_2_SECONDS = 2000000000ULL;

constexpr uint32_t GPU_ENTITY_SIZE = 128;

// Cache and Pool Sizes
constexpr uint32_t DEFAULT_MAX_DESCRIPTOR_SETS = 1024;
constexpr uint32_t DEFAULT_GRAPHICS_CACHE_SIZE = 1024;
constexpr uint32_t DEFAULT_COMPUTE_CACHE_SIZE = 512;
constexpr uint32_t DEFAULT_SHADER_CACHE_SIZE = 512;
constexpr uint32_t DEFAULT_LAYOUT_CACHE_SIZE = 256;
constexpr uint64_t CACHE_CLEANUP_INTERVAL = 1000;  // frames

// Compute Configuration
constexpr uint32_t THREADS_PER_WORKGROUP = 64;
constexpr uint32_t MAX_WORKGROUPS_PER_CHUNK = 512;

// Memory Sizes (in bytes)
constexpr size_t MEGABYTE = 1024 * 1024;
constexpr size_t STAGING_BUFFER_SIZE = 16 * MEGABYTE;
constexpr size_t MAX_CHUNK_SIZE = 8 * MEGABYTE;
constexpr size_t MIN_AVAILABLE_MEMORY = 500 * MEGABYTE;
constexpr size_t LARGE_BUFFER_THRESHOLD = 50 * MEGABYTE;

// Bindless Limits
constexpr uint32_t MAX_BINDLESS_TEXTURES = 16384;
constexpr uint32_t MAX_BINDLESS_BUFFERS = 8192;