#version 450

// Shadow pass vertex shader - depth-only rendering
layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Current cascade's light space matrix
    uint cascadeIndex;      // Which cascade we're rendering
} shadowUBO;

layout(push_constant) uniform PC {
    float time;
    float dt;
    uint  count;
    uint  cascadeIndex;  // Current cascade being rendered
} pc;

// Input vertex geometry
layout(location = 0) in vec3 inPos;

// Entity data buffers (same as main vertex shader)
layout(std430, binding = 1) readonly buffer ComputedPositions {
    vec4 computedPos[];
};

layout(std430, binding = 2) readonly buffer MovementParamsBuffer {
    vec4 movementParams[];  // amplitude, frequency, phase, timeOffset
} movementParamsBuffer;

void main() {
    // Read computed positions from physics shader output
    vec3 worldPos = computedPos[gl_InstanceIndex].xyz;
    
    // Extract movement parameters for transform calculation
    vec4 entityMovementParams = movementParamsBuffer.movementParams[gl_InstanceIndex];
    float phase = entityMovementParams.z;
    float timeOffset = entityMovementParams.w;
    float entityTime = pc.time + timeOffset;
    
    // Create same rotation matrix as main vertex shader for consistency
    float rotY = entityTime * 0.1;
    float rotX = entityTime * 0.05;
    float rotZ = entityTime * 0.07;
    
    // Rotation matrices for each axis
    mat4 rotationX = mat4(
        1,         0,          0, 0,
        0, cos(rotX), -sin(rotX), 0,
        0, sin(rotX),  cos(rotX), 0,
        0,         0,          0, 1
    );
    
    mat4 rotationY = mat4(
        cos(rotY),  0, sin(rotY), 0,
               0,  1,        0, 0,
       -sin(rotY),  0, cos(rotY), 0,
               0,  0,        0, 1
    );
    
    mat4 rotationZ = mat4(
        cos(rotZ), -sin(rotZ), 0, 0,
        sin(rotZ),  cos(rotZ), 0, 0,
               0,         0,  1, 0,
               0,         0,  0, 1
    );
    
    // Combined rotation matrix
    mat4 rotationMatrix = rotationZ * rotationY * rotationX;
    
    // Translation matrix
    mat4 translationMatrix = mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        worldPos, 1
    );
    
    // Final transformation: translate then rotate
    mat4 modelMatrix = translationMatrix * rotationMatrix;
    
    // Transform to light space (no perspective division here - done in fragment shader)
    gl_Position = shadowUBO.lightSpaceMatrix * modelMatrix * vec4(inPos, 1.0);
}