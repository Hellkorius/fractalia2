#pragma once

#include <cstdint>

/**
 * EntityDescriptorBindings - Constants for entity descriptor set layouts
 * 
 * Single Responsibility: Define binding layouts for entity rendering pipeline
 * - Compute shader bindings for SoA entity buffer structure
 * - Graphics shader bindings for rendering pipeline
 * - Centralized constants to eliminate magic numbers
 */
namespace EntityDescriptorBindings {

    // Compute descriptor set bindings (SoA structure)
    namespace Compute {
        enum Binding : uint32_t {
            VELOCITY_BUFFER = 0,
            MOVEMENT_PARAMS_BUFFER = 1,
            RUNTIME_STATE_BUFFER = 2,
            POSITION_BUFFER = 3,
            CURRENT_POSITION_BUFFER = 4,
            COLOR_BUFFER = 5,
            MODEL_MATRIX_BUFFER = 6,
            SPATIAL_MAP_BUFFER = 7
        };
        
        constexpr uint32_t BINDING_COUNT = 8;
    }

    // Graphics descriptor set bindings (rendering pipeline)
    namespace Graphics {
        enum Binding : uint32_t {
            UNIFORM_BUFFER = 0,      // Camera matrices
            POSITION_BUFFER = 1,     // Entity positions
            MOVEMENT_PARAMS_BUFFER = 2  // Movement params for color
        };
        
        constexpr uint32_t BINDING_COUNT = 3;
    }
}