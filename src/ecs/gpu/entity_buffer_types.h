#pragma once

#include <cstdint>

/**
 * Centralized entity buffer type definitions for Vulkan 1.3 descriptor indexing.
 * This replaces the old binding-based system with a single indexed buffer array.
 * 
 * All shaders use these same indices to access entity data through:
 * layout(std430, binding = 0) buffer EntityBuffers { vec4 data[]; } entityBuffers[];
 */

namespace EntityBufferType {
    // Core entity data buffers (SoA - Structure of Arrays)
    constexpr uint32_t VELOCITY = 0;           // vec4: velocity.xy, damping, reserved
    constexpr uint32_t MOVEMENT_PARAMS = 1;    // vec4: amplitude, frequency, phase, timeOffset
    constexpr uint32_t RUNTIME_STATE = 2;      // vec4: totalTime, initialized, stateTimer, entityState
    constexpr uint32_t ROTATION_STATE = 3;     // vec4: rotation, angularVelocity, angularDamping, reserved
    constexpr uint32_t COLOR = 4;              // vec4: RGBA color values
    constexpr uint32_t MODEL_MATRIX = 5;       // mat4: full 3D transform matrix (cold data)
    
    // Position and physics buffers
    constexpr uint32_t POSITION_OUTPUT = 6;    // vec4: computed positions for graphics (x,y,z,w)
    constexpr uint32_t CURRENT_POSITION = 7;   // vec4: physics integration state
    
    // Spatial optimization buffer
    constexpr uint32_t SPATIAL_MAP = 8;        // uvec2[]: spatial hash grid for collision detection
    
    // Reserved slots for future expansion
    constexpr uint32_t RESERVED_9 = 9;
    constexpr uint32_t RESERVED_10 = 10;
    constexpr uint32_t RESERVED_11 = 11;
    constexpr uint32_t RESERVED_12 = 12;
    constexpr uint32_t RESERVED_13 = 13;
    constexpr uint32_t RESERVED_14 = 14;
    constexpr uint32_t RESERVED_15 = 15;
    
    // Maximum number of entity buffers supported
    constexpr uint32_t MAX_ENTITY_BUFFERS = 16;
    
    // Buffer type validation
    constexpr bool isValidBufferType(uint32_t bufferType) {
        return bufferType < MAX_ENTITY_BUFFERS;
    }
    
    // Human-readable names for debugging
    constexpr const char* getBufferName(uint32_t bufferType) {
        switch (bufferType) {
            case VELOCITY: return "VelocityBuffer";
            case MOVEMENT_PARAMS: return "MovementParamsBuffer";
            case RUNTIME_STATE: return "RuntimeStateBuffer";
            case ROTATION_STATE: return "RotationStateBuffer";
            case COLOR: return "ColorBuffer";
            case MODEL_MATRIX: return "ModelMatrixBuffer";
            case POSITION_OUTPUT: return "PositionOutputBuffer";
            case CURRENT_POSITION: return "CurrentPositionBuffer";
            case SPATIAL_MAP: return "SpatialMapBuffer";
            default: return "ReservedBuffer";
        }
    }
}

/**
 * Legacy binding constants for compatibility during migration.
 * These will be removed once all systems use descriptor indexing.
 */
namespace LegacyEntityBindings {
    namespace Compute {
        constexpr uint32_t VELOCITY = 0;
        constexpr uint32_t MOVEMENT_PARAMS = 1;
        constexpr uint32_t RUNTIME_STATE = 2;
        constexpr uint32_t POSITION_OUTPUT = 3;
        constexpr uint32_t CURRENT_POSITION = 4;
        constexpr uint32_t ROTATION_STATE = 5;
        constexpr uint32_t SPATIAL_MAP = 7;
    }
    
    namespace Graphics {
        constexpr uint32_t UBO = 0;
        constexpr uint32_t COMPUTED_POSITIONS = 1;
        constexpr uint32_t MOVEMENT_PARAMS = 2;
        constexpr uint32_t ROTATION_STATE = 3;
    }
}