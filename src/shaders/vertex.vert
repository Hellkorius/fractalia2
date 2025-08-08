#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// GPU Entity instance data (matches GPUEntity struct from compute shader)
layout(location = 2) in vec4 instanceMatrix0;    // modelMatrix row 0
layout(location = 3) in vec4 instanceMatrix1;    // modelMatrix row 1  
layout(location = 4) in vec4 instanceMatrix2;    // modelMatrix row 2
layout(location = 5) in vec4 instanceMatrix3;    // modelMatrix row 3
layout(location = 6) in vec4 instanceColor;      // color
layout(location = 7) in vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 movementParams1;    // center.xyz, movementType
layout(location = 9) in vec4 runtimeState;       // totalTime, initialized, reserved, reserved

layout(location = 0) out vec3 fragColor;

void main() {
    mat4 instanceMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    gl_Position = ubo.proj * ubo.view * instanceMatrix * vec4(inPosition, 1.0);
    fragColor = instanceColor.rgb; // Use GPU-computed dynamic color
}