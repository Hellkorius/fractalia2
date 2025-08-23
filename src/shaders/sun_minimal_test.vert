#version 450

// Sun disc vertex shader - proper world space positioning using UBO
layout(location = 0) in vec2 inPos;

// Same UBO as entity rendering
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix0;
    mat4 lightSpaceMatrix1;  
    mat4 lightSpaceMatrix2;
    vec4 sunDirection;
    vec4 cascadeSplits;
    float shadowDistance;
    uint cascadeCount;
    float shadowBias;
    float shadowNormalOffset;
} ubo;

layout(location = 0) out vec4 fragColor;

void main() {
    // Get sun direction from UBO (same data used for lighting)
    vec3 sunLightDirection = ubo.sunDirection.xyz;  // Direction light comes FROM
    vec3 sunWorldPos = -sunLightDirection * 200.0;  // Sun is opposite to light direction
    float sunSize = 8.0;  // Reasonable size
    
    // Create billboard quad that always faces the camera
    // Get camera right and up vectors from view matrix
    vec3 cameraRight = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    vec3 cameraUp = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);
    
    // Position vertices around sun center using camera-aligned vectors
    vec3 worldPos = sunWorldPos + (cameraRight * inPos.x + cameraUp * inPos.y) * sunSize;
    
    // Transform to clip space with proper depth testing
    vec4 viewPos = ubo.view * vec4(worldPos, 1.0);
    gl_Position = ubo.proj * viewPos;
    
    // Cool, realistic sun color - more white/pale yellow than bright orange
    fragColor = vec4(1.0, 0.98, 0.9, 1.0);  // Pale, distant sun
}