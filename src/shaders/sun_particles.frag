#version 450

// Simplified sun particle fragment shader - no UBO needed for now
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float fragDepth;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Create circular particle shape with soft edges
    vec2 circleCoord = fragTexCoord * 2.0 - 1.0; // Convert to [-1,1] range
    float distance = length(circleCoord);
    
    // Soft circular falloff
    float alpha = 1.0 - smoothstep(0.5, 1.0, distance);
    
    // Use the input color with circular alpha
    outColor = vec4(fragColor.rgb, alpha * fragColor.a);
    
    // Discard completely transparent pixels
    if (outColor.a < 0.01) {
        discard;
    }
}