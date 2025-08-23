#version 450

// Sun system fragment shader - handles both sun disc and particles

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragDepth;

// Push constants to know what we're rendering
layout(push_constant) uniform PushConstants {
    int renderMode;  // 0 = sun disc, 1 = particles
    int instanceId;
} pc;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    if (pc.renderMode == 0) {
        // Render sun disc with radial gradient
        vec2 center = vec2(0.5, 0.5);
        float distFromCenter = length(fragTexCoord - center);
        
        // Create bright sun disc with soft edges
        float sunIntensity = 1.0 - smoothstep(0.3, 0.5, distFromCenter);
        sunIntensity = pow(sunIntensity, 2.0); // More concentrated center
        
        // Add sun corona effect
        float corona = 1.0 - smoothstep(0.4, 0.8, distFromCenter);
        corona *= 0.3;
        
        vec3 sunColor = fragColor.rgb;
        float totalIntensity = sunIntensity + corona;
        
        outColor = vec4(sunColor * totalIntensity, totalIntensity * 0.9);
        
    } else {
        // Render particle with soft circular gradient
        vec2 center = vec2(0.5, 0.5);
        float distFromCenter = length(fragTexCoord - center);
        
        // Soft circular particle
        float particleAlpha = 1.0 - smoothstep(0.0, 0.5, distFromCenter);
        particleAlpha = pow(particleAlpha, 1.5); // Softer edges
        
        // Apply particle color and brightness
        vec3 particleColor = fragColor.rgb;
        float brightness = fragColor.a;
        
        outColor = vec4(particleColor * brightness, particleAlpha * brightness);
    }
    
    // Ensure we don't output completely transparent pixels (can cause issues)
    if (outColor.a < 0.01) {
        discard;
    }
}