#version 450

// God rays fragment shader - creates volumetric light shafts
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in float fragRayIntensity;
layout(location = 3) in vec2 fragRayCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 sunDirection;
    vec4 sunPosition;
    vec4 sceneCenter;
    float time;
    float rayLength;
    float rayWidth;
    uint numRays;
} ubo;

// Create volumetric ray appearance
float createVolumetricRay(vec2 rayCoord) {
    // Distance from center of ray (across the width)
    float crossDistance = abs(rayCoord.y - 0.5) * 2.0; // 0 at center, 1 at edges
    
    // Distance along the ray (from start to end)
    float alongDistance = rayCoord.x; // 0 at start, 1 at end
    
    // Soft falloff across ray width
    float widthFalloff = 1.0 - smoothstep(0.0, 1.0, crossDistance);
    widthFalloff = pow(widthFalloff, 0.5); // Softer falloff
    
    // Intensity variation along ray length
    float lengthProfile = 1.0 - alongDistance * 0.7; // Stronger at start, weaker at end
    
    // Add noise for organic look
    float noise = sin(rayCoord.x * 15.0 + ubo.time) * sin(rayCoord.y * 8.0 + ubo.time * 0.7) * 0.1;
    float organicVariation = 1.0 + noise;
    
    // Atmospheric scattering effect
    float scattering = 1.0 - exp(-alongDistance * 2.0); // More scattering toward the end
    
    return widthFalloff * lengthProfile * organicVariation * (0.7 + scattering * 0.3);
}

// Add dust motes and particle effects in the rays
float addDustMotes(vec2 rayCoord) {
    float motes = 0.0;
    
    // Several layers of dust motes at different scales
    for (int i = 0; i < 3; i++) {
        float scale = pow(2.0, float(i));
        float timeOffset = float(i) * 1.5;
        
        vec2 moteCoord = rayCoord * scale + vec2(ubo.time * 0.1 + timeOffset);
        
        // Simple procedural dust motes
        float motePattern = sin(moteCoord.x * 20.0) * sin(moteCoord.y * 20.0);
        motePattern = pow(max(0.0, motePattern), 3.0);
        
        motes += motePattern * (0.5 / scale); // Smaller contribution from larger scales
    }
    
    return motes;
}

// Atmospheric perspective based on distance
vec3 applyAtmosphericPerspective(vec3 color, float distance) {
    // Simple atmospheric scattering
    float fogFactor = exp(-distance * 0.0002);
    vec3 atmosColor = vec3(0.6, 0.7, 0.9); // Sky color
    
    return mix(atmosColor, color, fogFactor);
}

void main() {
    // Early discard for performance - very transparent areas
    if (fragRayIntensity < 0.01) {
        discard;
    }
    
    // Create volumetric ray appearance
    float rayShape = createVolumetricRay(fragRayCoord);
    
    // Early discard if ray shape is too weak
    if (rayShape < 0.01) {
        discard;
    }
    
    // Add dust motes and atmospheric effects
    float dustMotes = addDustMotes(fragRayCoord);
    
    // Calculate distance from camera for atmospheric effects
    vec3 cameraPos = -vec3(ubo.view[3]);
    float distanceFromCamera = length(fragWorldPos - cameraPos);
    
    // Apply atmospheric perspective
    vec3 finalColor = applyAtmosphericPerspective(fragColor.rgb, distanceFromCamera);
    
    // Combine ray shape, dust motes, and base intensity
    float finalIntensity = fragRayIntensity * rayShape * (1.0 + dustMotes * 0.5);
    
    // Enhanced brightness for dramatic effect
    finalColor *= (1.0 + finalIntensity);
    
    // Final alpha calculation - more transparent at edges, more opaque at center
    float finalAlpha = fragColor.a * rayShape * (0.3 + fragRayIntensity * 0.7);
    
    // Distance-based alpha falloff
    float distanceAlpha = 1.0 / (1.0 + distanceFromCamera * 0.0005);
    finalAlpha *= distanceAlpha;
    
    // Clamp alpha for proper blending
    finalAlpha = clamp(finalAlpha, 0.0, 0.6);
    
    outColor = vec4(finalColor, finalAlpha);
}