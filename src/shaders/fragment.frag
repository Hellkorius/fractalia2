#version 450

layout(location = 0) in vec3 color;
layout(location = 1) in vec4 worldPos;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec4 lightSpacePos0;
layout(location = 4) in vec4 lightSpacePos1;
layout(location = 5) in vec4 lightSpacePos2;

layout(location = 0) out vec4 outColor;

// Shadow map samplers for each cascade
layout(binding = 3) uniform sampler2D shadowMap0;
layout(binding = 4) uniform sampler2D shadowMap1;
layout(binding = 5) uniform sampler2D shadowMap2;

// UBO for shadow parameters (same as vertex shader)
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

// PCF shadow sampling with 3x3 kernel
float sampleShadowMap(sampler2D shadowMap, vec2 uv, float compareDepth, float bias) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return 1.0; // Outside shadow map bounds = lit
    }
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // 3x3 PCF kernel for smooth shadows
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float pcfDepth = texture(shadowMap, uv + offset).r;
            shadow += (compareDepth - bias > pcfDepth) ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

// Calculate shadow factor using cascaded shadow maps
float calculateShadowFactor() {
    // Calculate distance from camera for cascade selection
    float depth = length(worldPos.xyz - (inverse(ubo.view) * vec4(0, 0, 0, 1)).xyz);
    float cascadeDepth = depth / ubo.shadowDistance;
    
    // Select appropriate cascade based on depth and sample shadow map
    if (cascadeDepth <= ubo.cascadeSplits.x && ubo.cascadeCount > 0) {
        // First cascade (closest)
        vec3 projCoords = lightSpacePos0.xyz / lightSpacePos0.w;
        projCoords = projCoords * 0.5 + 0.5;
        float cosTheta = dot(normalize(worldNormal), normalize(-ubo.sunDirection.xyz));
        float bias = max(ubo.shadowBias * (1.0 - cosTheta), ubo.shadowBias * 0.1);
        return sampleShadowMap(shadowMap0, projCoords.xy, projCoords.z, bias);
    } else if (cascadeDepth <= ubo.cascadeSplits.y && ubo.cascadeCount > 1) {
        // Second cascade
        vec3 projCoords = lightSpacePos1.xyz / lightSpacePos1.w;
        projCoords = projCoords * 0.5 + 0.5;
        float cosTheta = dot(normalize(worldNormal), normalize(-ubo.sunDirection.xyz));
        float bias = max(ubo.shadowBias * (1.0 - cosTheta), ubo.shadowBias * 0.1);
        return sampleShadowMap(shadowMap1, projCoords.xy, projCoords.z, bias);
    } else if (cascadeDepth <= ubo.cascadeSplits.z && ubo.cascadeCount > 2) {
        // Third cascade (farthest)
        vec3 projCoords = lightSpacePos2.xyz / lightSpacePos2.w;
        projCoords = projCoords * 0.5 + 0.5;
        float cosTheta = dot(normalize(worldNormal), normalize(-ubo.sunDirection.xyz));
        float bias = max(ubo.shadowBias * (1.0 - cosTheta), ubo.shadowBias * 0.1);
        return sampleShadowMap(shadowMap2, projCoords.xy, projCoords.z, bias);
    } else {
        // Beyond shadow distance
        return 0.3; // Ambient lighting only
    }
}

// Calculate lighting with sun direction and interesting angle
vec3 calculateLighting(vec3 baseColor) {
    vec3 normal = normalize(worldNormal);
    vec3 lightDir = normalize(-ubo.sunDirection.xyz);
    
    // Diffuse lighting with interesting sun angle
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Calculate shadow factor (simplified - no shadow maps for now)
    float shadowFactor = 0.8f; // Most areas are lit, some shadow for depth
    
    // Very dark ambient lighting - most things should be in shadow
    vec3 ambient = baseColor * vec3(0.05, 0.05, 0.1) * 0.1;
    
    // Bright, warm sun lighting - only things facing the sun are lit
    vec3 sunColor = vec3(1.8, 1.4, 1.0) * ubo.sunDirection.w; // w component = intensity
    vec3 direct = baseColor * sunColor * NdotL * shadowFactor;
    
    // Subtle rim lighting for depth perception
    vec3 viewDir = normalize((inverse(ubo.view) * vec4(0, 0, 0, 1)).xyz - worldPos.xyz);
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    vec3 rimLight = sunColor * pow(rim, 3.0) * 0.2 * shadowFactor;
    
    return ambient + direct + rimLight;
}

// Add visible sun as direct light source that camera can "absorb"
vec3 addVisibleSun(vec3 baseColor, vec3 worldPosition) {
    vec3 cameraPos = (inverse(ubo.view) * vec4(0, 0, 0, 1)).xyz;
    vec3 viewDir = normalize(worldPosition - cameraPos);
    
    // Sun position in world space (same as sun system: (0, 50, 0))
    vec3 sunPos = vec3(0.0, 50.0, 0.0);
    vec3 toSun = normalize(sunPos - cameraPos);
    
    // Calculate how directly we're looking at the sun
    float sunAlignment = max(0.0, dot(viewDir, toSun));
    
    // Make sun visible as bright disc when looking toward it
    if (sunAlignment > 0.998) { // Sun disc - very narrow cone
        float sunIntensity = pow((sunAlignment - 0.998) / 0.002, 0.3);
        vec3 sunColor = vec3(5.0, 4.0, 2.5) * ubo.sunDirection.w; // Extremely bright
        return baseColor + sunColor * sunIntensity;
    }
    
    // Sun corona/halo - wider visible area
    if (sunAlignment > 0.995) {
        float haloIntensity = pow((sunAlignment - 0.995) / 0.003, 1.0);
        vec3 haloColor = vec3(3.0, 2.0, 1.0) * ubo.sunDirection.w * 0.7;
        return baseColor + haloColor * haloIntensity;
    }
    
    // Soft glow around sun
    if (sunAlignment > 0.99) {
        float glowIntensity = pow((sunAlignment - 0.99) / 0.005, 2.0);
        vec3 glowColor = vec3(1.5, 1.2, 0.8) * ubo.sunDirection.w * 0.3;
        return baseColor + glowColor * glowIntensity;
    }
    
    return baseColor;
}

void main() {
    // Apply lighting to the base color
    vec3 litColor = calculateLighting(color);
    
    // Add visible sun in the sky
    litColor = addVisibleSun(litColor, worldPos.xyz);
    
    // Optional: Add atmospheric perspective for distant objects
    float depth = length(worldPos.xyz - (inverse(ubo.view) * vec4(0, 0, 0, 1)).xyz);
    float fogFactor = exp(-depth * 0.0001); // Very subtle fog
    litColor = mix(vec3(0.6, 0.7, 0.9), litColor, fogFactor); // Mix with sky color
    
    outColor = vec4(litColor, 1.0);
}