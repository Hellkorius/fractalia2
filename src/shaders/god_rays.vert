#version 450

// God rays vertex shader - creates dramatic light shafts from the sun
layout(location = 0) in vec3 inPos;  // Ray geometry vertices

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 sunDirection;       // xyz = direction, w = intensity
    vec4 sunPosition;        // xyz = position in world space, w = spread
    vec4 sceneCenter;        // xyz = scene center, w = radius
    float time;
    float rayLength;
    float rayWidth;
    uint numRays;
} ubo;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out float fragRayIntensity;
layout(location = 3) out vec2 fragRayCoord; // Position along ray (x) and across ray (y)

void main() {
    uint rayIndex = gl_InstanceIndex;
    
    // Calculate ray direction with spread
    vec3 baseSunDir = normalize(ubo.sunDirection.xyz);
    
    // Create cone of rays around main sun direction
    float rayAngle = (float(rayIndex) / float(ubo.numRays)) * 6.283185 * 2.0; // Multiple rotations
    float coneRadius = ubo.sunPosition.w * 0.3; // Spread factor
    
    // Create ray direction within cone
    vec3 perpendicular1 = normalize(cross(baseSunDir, vec3(0, 1, 0)));
    vec3 perpendicular2 = cross(baseSunDir, perpendicular1);
    vec3 rayOffset = perpendicular1 * cos(rayAngle) * coneRadius + 
                     perpendicular2 * sin(rayAngle) * coneRadius;
    vec3 rayDirection = normalize(baseSunDir + rayOffset);
    
    // Start rays from far above the scene
    vec3 rayStart = ubo.sceneCenter.xyz - rayDirection * ubo.rayLength * 0.5;
    
    // Create ray geometry - each ray is a long quad
    vec3 rayEnd = rayStart + rayDirection * ubo.rayLength;
    
    // Calculate ray width based on distance and perspective
    vec3 cameraPos = -vec3(ubo.view[3][0], ubo.view[3][1], ubo.view[3][2]);
    float rayDistFromCamera = length(mix(rayStart, rayEnd, 0.5) - cameraPos);
    float perspectiveWidth = ubo.rayWidth * (1.0 + rayDistFromCamera * 0.001);
    
    // Create ray cross-section vectors
    vec3 rayRight = normalize(cross(rayDirection, vec3(0, 1, 0))) * perspectiveWidth;
    vec3 rayUp = cross(rayDirection, rayRight) * perspectiveWidth * 0.1; // Flatter rays
    
    // Position vertex along the ray quad
    vec3 rayPos;
    if (gl_VertexIndex == 0) {
        rayPos = rayStart - rayRight - rayUp;
        fragRayCoord = vec2(0.0, 0.0);
    } else if (gl_VertexIndex == 1) {
        rayPos = rayStart + rayRight - rayUp;
        fragRayCoord = vec2(0.0, 1.0);
    } else if (gl_VertexIndex == 2) {
        rayPos = rayEnd - rayRight + rayUp;
        fragRayCoord = vec2(1.0, 0.0);
    } else {
        rayPos = rayEnd + rayRight + rayUp;
        fragRayCoord = vec2(1.0, 1.0);
    }
    
    // Transform to clip space
    gl_Position = ubo.proj * ubo.view * vec4(rayPos, 1.0);
    
    // Calculate ray intensity based on angle to main sun direction
    float rayAlignment = max(0.0, dot(rayDirection, baseSunDir));
    float rayIntensity = pow(rayAlignment, 2.0) * ubo.sunDirection.w;
    
    // Animated intensity variation
    float timeVariation = sin(ubo.time * 2.0 + float(rayIndex) * 0.5) * 0.2 + 0.8;
    rayIntensity *= timeVariation;
    
    // Ray color - golden sunlight
    vec3 rayColor = mix(vec3(1.0, 0.8, 0.4), vec3(1.0, 1.0, 0.7), rayAlignment);
    fragColor = vec4(rayColor, rayIntensity * 0.3); // Semi-transparent
    
    fragWorldPos = rayPos;
    fragRayIntensity = rayIntensity;
}