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
    uint bufferLength;      // Total keyframe buffer length (20)
    float totalTime;        // Current simulation time
    float deltaTime;        // Time per frame
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
    
    // Calculate frame index and interpolation alpha using rolling indexing
    float frameIndex = pc.totalTime / pc.deltaTime;
    uint currentFrameIndex = uint(frameIndex) % pc.bufferLength;
    uint nextFrameIndex = (currentFrameIndex + 1) % pc.bufferLength;
    float alpha = fract(frameIndex);
    
    // Calculate keyframe base indices
    uint baseIndex1 = (currentFrameIndex * pc.entityCount + entityID) * 8;
    uint baseIndex2 = (nextFrameIndex * pc.entityCount + entityID) * 8;
    
    // Read current keyframe data: position(3) + rotation(1) + color(4)
    vec3 keyframePos1 = vec3(keyframes[baseIndex1 + 0], keyframes[baseIndex1 + 1], keyframes[baseIndex1 + 2]);
    float keyframeRotation1 = keyframes[baseIndex1 + 3];
    vec4 keyframeColor1 = vec4(keyframes[baseIndex1 + 4], keyframes[baseIndex1 + 5], keyframes[baseIndex1 + 6], keyframes[baseIndex1 + 7]);
    
    // Read next keyframe data: position(3) + rotation(1) + color(4)
    vec3 keyframePos2 = vec3(keyframes[baseIndex2 + 0], keyframes[baseIndex2 + 1], keyframes[baseIndex2 + 2]);
    float keyframeRotation2 = keyframes[baseIndex2 + 3];
    vec4 keyframeColor2 = vec4(keyframes[baseIndex2 + 4], keyframes[baseIndex2 + 5], keyframes[baseIndex2 + 6], keyframes[baseIndex2 + 7]);
    
    // Interpolate between keyframes
    vec3 interpolatedPos = mix(keyframePos1, keyframePos2, alpha);
    float interpolatedRotation = mix(keyframeRotation1, keyframeRotation2, alpha);
    vec4 interpolatedColor = mix(keyframeColor1, keyframeColor2, alpha);
    
    // Build transform matrix with interpolated rotation and position
    float cosR = cos(interpolatedRotation);
    float sinR = sin(interpolatedRotation);
    
    mat4 keyframeMatrix = mat4(
        cosR, sinR, 0.0, 0.0,
        -sinR, cosR, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        interpolatedPos.x, interpolatedPos.y, interpolatedPos.z, 1.0
    );
    
    gl_Position = ubo.proj * ubo.view * keyframeMatrix * vec4(inPosition, 1.0);
    fragColor = interpolatedColor.rgb; // Use interpolated color
}