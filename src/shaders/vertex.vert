#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Keyframe buffer for position + rotation + color data
layout(binding = 1, std430) readonly buffer KeyframeBuffer {
    float keyframes[]; // Raw float array: position(3) + rotation(1) + color(4) per keyframe
};

// Push constants for keyframe interpolation
layout(push_constant) uniform PushConstants {
    float totalTime;        // Current simulation time
    float deltaTime;        // Time per frame
    float predictionTime;   // Time the keyframes were predicted for
    uint entityCount;       // Total number of entities
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// GPU Entity instance data (matches GPUEntity struct from compute shader)
layout(location = 2) in vec4 instanceMatrix0;    // modelMatrix row 0 (for rotation/scale only)
layout(location = 3) in vec4 instanceMatrix1;    // modelMatrix row 1  
layout(location = 4) in vec4 instanceMatrix2;    // modelMatrix row 2
layout(location = 5) in vec4 instanceMatrix3;    // modelMatrix row 3 (position ignored, using keyframes)
layout(location = 6) in vec4 instanceColor;      // color
layout(location = 7) in vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 movementParams1;    // center.xyz, movementType
layout(location = 9) in vec4 runtimeState;       // totalTime, initialized, reserved, reserved

layout(location = 0) out vec3 fragColor;

void main() {
    // Get entity ID from instance index
    uint entityID = gl_InstanceIndex;
    
    // Calculate interpolation alpha: how far from current time to prediction time
    float alpha = (pc.totalTime - (pc.predictionTime - 20.0 * pc.deltaTime)) / (20.0 * pc.deltaTime);
    alpha = clamp(alpha, 0.0, 1.0);
    
    // Read predicted keyframe data
    uint baseIndex = entityID * 8;
    vec3 predictedPos = vec3(keyframes[baseIndex + 0], keyframes[baseIndex + 1], keyframes[baseIndex + 2]);
    float predictedRotation = keyframes[baseIndex + 3];
    vec4 predictedColor = vec4(keyframes[baseIndex + 4], keyframes[baseIndex + 5], keyframes[baseIndex + 6], keyframes[baseIndex + 7]);
    
    // Compute current position using same movement logic as compute shader
    // For now, use the predicted values directly (no interpolation)
    // TODO: Add real-time position computation for smooth interpolation
    
    // Build transform matrix
    float cosR = cos(predictedRotation);
    float sinR = sin(predictedRotation);
    
    mat4 keyframeMatrix = mat4(
        cosR, sinR, 0.0, 0.0,
        -sinR, cosR, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        predictedPos.x, predictedPos.y, predictedPos.z, 1.0
    );
    
    gl_Position = ubo.proj * ubo.view * keyframeMatrix * vec4(inPosition, 1.0);
    fragColor = predictedColor.rgb;
}