#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec4 instanceMatrix0;
layout(location = 3) in vec4 instanceMatrix1;
layout(location = 4) in vec4 instanceMatrix2;
layout(location = 5) in vec4 instanceMatrix3;

layout(location = 0) out vec3 fragColor;

void main() {
    mat4 instanceMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    gl_Position = ubo.proj * ubo.view * instanceMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
}