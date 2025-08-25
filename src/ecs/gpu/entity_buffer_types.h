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
    constexpr uint32_t VELOCITY = 0;           // vec4: velocity.xyz, damping
    constexpr uint32_t MOVEMENT_PARAMS = 1;    // vec4: amplitude, frequency, phase, timeOffset
    constexpr uint32_t MOVEMENT_CENTERS = 2;   // vec4: movement center.xyz, reserved
    constexpr uint32_t RUNTIME_STATE = 3;      // vec4: totalTime, initialized, stateTimer, entityState
    constexpr uint32_t ROTATION_STATE = 4;     // vec4: rotation, angularVelocity, angularDamping, reserved
    constexpr uint32_t COLOR = 5;              // vec4: RGBA color values
    constexpr uint32_t MODEL_MATRIX = 6;       // mat4: full 3D transform matrix (physics writes to column 3)
    
    // Spatial optimization buffer
    constexpr uint32_t SPATIAL_MAP = 7;        // uvec2[]: spatial hash grid for collision detection
    
    // DEPRECATED: Position buffers are now unused - physics writes to MODEL_MATRIX column 3
    constexpr uint32_t POSITION_OUTPUT = 8;    // DEPRECATED: use MODEL_MATRIX buffer column 3
    constexpr uint32_t CURRENT_POSITION = 9;   // DEPRECATED: use MODEL_MATRIX buffer column 3
    
    // Reserved slots for future expansion
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
            case MOVEMENT_CENTERS: return "MovementCentersBuffer";
            case RUNTIME_STATE: return "RuntimeStateBuffer";
            case ROTATION_STATE: return "RotationStateBuffer";
            case COLOR: return "ColorBuffer";
            case MODEL_MATRIX: return "ModelMatrixBuffer";
            case SPATIAL_MAP: return "SpatialMapBuffer";
            case POSITION_OUTPUT: return "PositionOutputBuffer (DEPRECATED)";
            case CURRENT_POSITION: return "CurrentPositionBuffer (DEPRECATED)";
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