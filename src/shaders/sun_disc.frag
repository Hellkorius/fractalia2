#version 450

// Sun disc fragment shader - creates a bright, glowing sun
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in float fragDistanceToSun;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 sunDirection;
    vec4 sunPosition;
    float time;
    float sunGlowRadius;
    float sunCoreRadius;
    float atmosScattering;
} ubo;

// Create dramatic sun disc with multiple layers
vec4 createSunDisc(vec2 uv) {
    vec2 center = vec2(0.5);
    float dist = length(uv - center);
    
    // Multiple concentric layers for epic sun effect
    
    // Core: Bright white center
    float coreRadius = 0.05;
    float core = 1.0 - smoothstep(0.0, coreRadius, dist);
    vec3 coreColor = vec3(2.0, 2.0, 1.8); // Very bright white-yellow
    
    // Inner ring: Intense yellow-orange
    float innerRadius = 0.15;
    float innerRing = (1.0 - smoothstep(coreRadius, innerRadius, dist)) * 
                      smoothstep(0.0, coreRadius * 0.5, dist);
    vec3 innerColor = vec3(2.5, 1.8, 0.8); // Bright yellow-orange
    
    // Middle ring: Orange-red gradient
    float middleRadius = 0.3;
    float middleRing = (1.0 - smoothstep(innerRadius, middleRadius, dist)) * 
                       smoothstep(coreRadius, innerRadius * 1.2, dist);
    vec3 middleColor = vec3(2.0, 1.2, 0.4); // Orange
    
    // Outer glow: Red-orange fade
    float outerRadius = 0.5;
    float outerGlow = (1.0 - smoothstep(middleRadius, outerRadius, dist)) * 
                      smoothstep(innerRadius, middleRadius * 1.1, dist);
    vec3 outerColor = vec3(1.5, 0.8, 0.3); // Red-orange
    
    // Atmospheric glow: Soft blue-white around edges
    float atmosRadius = 0.7;
    float atmosGlow = (1.0 - smoothstep(outerRadius, atmosRadius, dist)) * 
                      smoothstep(middleRadius, outerRadius * 1.1, dist);
    vec3 atmosColor = vec3(1.2, 1.0, 0.9); // Soft white
    
    // Sun spots and surface detail (animated)
    float spotNoise = sin(uv.x * 20.0 + ubo.time) * sin(uv.y * 15.0 + ubo.time * 1.3) * 0.1;
    float surfaceDetail = 1.0 + spotNoise;
    
    // Combine all layers
    vec3 finalColor = coreColor * core * surfaceDetail + 
                      innerColor * innerRing * surfaceDetail +
                      middleColor * middleRing * surfaceDetail +
                      outerColor * outerGlow +
                      atmosColor * atmosGlow;
    
    // Total alpha for blending
    float totalAlpha = core + innerRing + middleRing + outerGlow + atmosGlow * 0.5;
    
    return vec4(finalColor, min(totalAlpha, 1.0));
}

// Add corona effects and solar flares
vec4 addCoronaEffects(vec4 baseColor, vec2 uv) {
    vec2 center = vec2(0.5);
    float dist = length(uv - center);
    
    // Rotating corona streamers
    float angle = atan(uv.y - center.y, uv.x - center.x);
    float coronaStrength = 0.0;
    
    // Multiple rotating streamers
    for (int i = 0; i < 8; i++) {
        float streamerAngle = float(i) * 0.785398 + ubo.time * 0.2; // 45 degrees apart
        float angleDiff = abs(angle - streamerAngle);
        angleDiff = min(angleDiff, 6.283185 - angleDiff); // Wrap around
        
        if (angleDiff < 0.1) { // Narrow streamers
            float streamerIntensity = (1.0 - smoothstep(0.0, 0.1, angleDiff)) * 
                                     (1.0 - smoothstep(0.3, 1.0, dist)) *
                                     smoothstep(0.15, 0.3, dist);
            coronaStrength += streamerIntensity * 0.3;
        }
    }
    
    vec3 coronaColor = vec3(1.8, 1.2, 0.6); // Golden corona
    baseColor.rgb += coronaColor * coronaStrength;
    baseColor.a += coronaStrength * 0.5;
    
    return baseColor;
}

void main() {
    // Create the main sun disc
    vec4 sunColor = createSunDisc(fragTexCoord);
    
    // Add corona and solar flares
    sunColor = addCoronaEffects(sunColor, fragTexCoord);
    
    // Distance-based intensity adjustment
    float distanceFactor = 1.0 / (1.0 + fragDistanceToSun * 0.0001);
    sunColor.rgb *= distanceFactor;
    
    // Bloom effect - make very bright for post-processing bloom
    sunColor.rgb *= ubo.sunDirection.w; // Intensity multiplier
    
    // Ensure sun is always visible
    sunColor.a = clamp(sunColor.a, 0.1, 1.0);
    
    outColor = sunColor;
}