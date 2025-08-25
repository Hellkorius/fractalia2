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

    // Compute descriptor set bindings (SoA structure) - DEPRECATED
    // Modern shaders use Vulkan 1.3 descriptor indexing instead
    namespace Compute {
        enum Binding : uint32_t {
            VELOCITY_BUFFER = 0,
            MOVEMENT_PARAMS_BUFFER = 1,
            MOVEMENT_CENTERS_BUFFER = 2,
            RUNTIME_STATE_BUFFER = 3,
            // DEPRECATED: Position buffers removed - use MODEL_MATRIX column 3
            ROTATION_STATE_BUFFER = 4,
            COLOR_BUFFER = 5,
            MODEL_MATRIX_BUFFER = 6,
            SPATIAL_MAP_BUFFER = 7,
            SPATIAL_ENTITIES_BUFFER = 8
        };
        
        constexpr uint32_t BINDING_COUNT = 9; // Reduced from 11 after removing position buffers
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