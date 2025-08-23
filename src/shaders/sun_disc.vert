#version 450

// Sun disc vertex shader - creates a bright sun object in the sky
layout(location = 0) in vec2 inPos;  // Quad vertices (-1,-1 to 1,1)

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 sunDirection;       // xyz = direction, w = intensity
    vec4 sunPosition;        // xyz = position in world space, w = size
    float time;
    float sunGlowRadius;
    float sunCoreRadius;
    float atmosScattering;
} ubo;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out float fragDistanceToSun;

void main() {
    // Calculate sun position in sky (far away from camera)
    vec3 cameraPos = -vec3(ubo.view[3][0], ubo.view[3][1], ubo.view[3][2]);
    vec3 sunWorldPos = cameraPos - normalize(ubo.sunDirection.xyz) * 500.0; // Far in sun direction
    
    // Create billboard facing camera
    vec3 toCameraDir = normalize(cameraPos - sunWorldPos);
    vec3 upDir = vec3(0, 1, 0);
    vec3 rightDir = normalize(cross(toCameraDir, upDir));
    upDir = cross(rightDir, toCameraDir);
    
    // Scale sun quad based on configured size
    float sunSize = ubo.sunPosition.w * 20.0; // Make it big and dramatic
    
    // Pulsing effect for dramatic sun
    float pulseFactor = 1.0 + sin(ubo.time * 2.0) * 0.1;
    sunSize *= pulseFactor;
    
    // Create sun quad
    vec3 quadPos = sunWorldPos + 
                   rightDir * inPos.x * sunSize + 
                   upDir * inPos.y * sunSize;
    
    // Transform to clip space
    vec4 clipPos = ubo.proj * ubo.view * vec4(quadPos, 1.0);
    gl_Position = clipPos;
    
    // Pass data to fragment shader
    fragTexCoord = inPos * 0.5 + 0.5; // Convert from [-1,1] to [0,1]
    fragWorldPos = quadPos;
    fragDistanceToSun = length(cameraPos - sunWorldPos);
}