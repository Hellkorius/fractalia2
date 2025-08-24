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
            MOVEMENT_CENTERS_BUFFER = 2,
            RUNTIME_STATE_BUFFER = 3,
            POSITION_BUFFER = 4,
            CURRENT_POSITION_BUFFER = 5,
            ROTATION_STATE_BUFFER = 6,
            COLOR_BUFFER = 7,
            MODEL_MATRIX_BUFFER = 8,
            SPATIAL_MAP_BUFFER = 9
        };
        
        constexpr uint32_t BINDING_COUNT = 10;
    }

    // Graphics descriptor set bindings (rendering pipeline)
    namespace Graphics {
        enum Binding : uint32_t {
            UNIFORM_BUFFER = 0,      // Camera matrices
            POSITION_BUFFER = 1,     // Entity positions
            MOVEMENT_PARAMS_BUFFER = 2,   // Movement params for color
            MOVEMENT_CENTERS_BUFFER = 3,  // Movement centers for 3D support
            ROTATION_STATE_BUFFER = 4     // Rotation state for transforms
        };
        
        constexpr uint32_t BINDING_COUNT = 5;
    }
}