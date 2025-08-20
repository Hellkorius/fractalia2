#pragma once

#include <cstddef>

// System-wide constants for the ECS framework
namespace SystemConstants {
    // Entity batch sizes
    constexpr size_t DEFAULT_ENTITY_BATCH_SIZE = 1000;
    constexpr size_t MIN_ENTITY_RESERVE_COUNT = 1000;
    
    // Movement type constants - simplified to only random walk
    constexpr int MOVEMENT_TYPE_RANDOM_WALK = 0;
}