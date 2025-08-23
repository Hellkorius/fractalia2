#version 450

// Sun particle vertex shader with proper descriptor bindings
layout(location = 0) in vec2 inPos;  // Quad vertices (-1,-1 to 1,1)

// TEMPORARY: Remove all descriptor bindings for minimal test

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out float fragDepth;
layout(location = 3) out vec3 fragWorldPos;

void main() {
    // Simple screen-space quad (no UBO)
    vec2 screenPos = inPos * 0.2; // Make it smaller so we can see it
    gl_Position = vec4(screenPos, 0.0, 1.0);
    
    // Simple golden particle color
    fragColor = vec4(1.0, 0.8, 0.4, 0.8);
    fragTexCoord = inPos * 0.5 + 0.5; // Convert from [-1,1] to [0,1]
    fragDepth = 0.0;
    fragWorldPos = vec3(screenPos, 0.0);
}