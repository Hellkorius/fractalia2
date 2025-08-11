#pragma once

#include <cstddef>

// System-wide constants for the ECS framework
namespace SystemConstants {
    // Entity batch sizes
    constexpr size_t DEFAULT_ENTITY_BATCH_SIZE = 1000;
    constexpr size_t MIN_ENTITY_RESERVE_COUNT = 1000;
    
    // Movement type constants
    constexpr int MOVEMENT_TYPE_PETAL = 0;
    constexpr int MOVEMENT_TYPE_ORBIT = 1; 
    constexpr int MOVEMENT_TYPE_WAVE = 2;
    constexpr int MOVEMENT_TYPE_TRIANGLE = 3;
}