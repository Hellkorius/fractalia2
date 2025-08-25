#ifndef ENTITY_BUFFER_INDICES_GLSL
#define ENTITY_BUFFER_INDICES_GLSL

/**
 * Shared entity buffer indices for Vulkan 1.3 descriptor indexing.
 * This header is included by all shaders to ensure consistent buffer access.
 * 
 * Usage in shaders:
 * #include "entity_buffer_indices.glsl"
 * 
 * vec4 velocity = entityBuffers[VELOCITY_BUFFER].data[entityIndex];
 * vec4 rotation = entityBuffers[ROTATION_STATE_BUFFER].data[entityIndex];
 */

// Core entity data buffer indices (matches EntityBufferType in C++)
const uint VELOCITY_BUFFER = 0u;           // vec4: velocity.xy, damping, reserved
const uint MOVEMENT_PARAMS_BUFFER = 1u;    // vec4: amplitude, frequency, phase, timeOffset
const uint MOVEMENT_CENTERS_BUFFER = 2u;   // vec4: movement center.xyz, reserved
const uint RUNTIME_STATE_BUFFER = 3u;      // vec4: totalTime, initialized, stateTimer, entityState
const uint ROTATION_STATE_BUFFER = 4u;     // vec4: rotation, angularVelocity, angularDamping, reserved
const uint COLOR_BUFFER = 5u;              // vec4: RGBA color values
const uint MODEL_MATRIX_BUFFER = 6u;       // mat4: full 3D transform matrix

// NOTE: Spatial buffers (SPATIAL_MAP, SPATIAL_ENTITIES) use dedicated bindings (2, 10)
// They are NOT part of the entity buffer array due to type requirements (uvec2, uint vs vec4)

// DEPRECATED: Position buffers are now unused - physics writes to MODEL_MATRIX_BUFFER
// These constants are kept for compatibility during migration  
const uint POSITION_OUTPUT_BUFFER = 7u;    // DEPRECATED: use MODEL_MATRIX_BUFFER column 3
const uint CURRENT_POSITION_BUFFER = 8u;   // DEPRECATED: use MODEL_MATRIX_BUFFER column 3

// Reserved slots for future expansion
const uint RESERVED_9 = 9u;
const uint RESERVED_10 = 10u;
const uint RESERVED_11 = 11u;

// Maximum number of buffers
const uint MAX_ENTITY_BUFFERS = 16u;

// Buffer validation helper (compile-time check)
bool isValidBuffer(uint bufferIndex) {
    return bufferIndex < MAX_ENTITY_BUFFERS;
}

#endif // ENTITY_BUFFER_INDICES_GLSL