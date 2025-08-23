#version 450

// Minimal test particle vertex shader - no UBO or storage buffers needed
layout(location = 0) in vec2 inPos;  // Quad vertices (-1,-1 to 1,1)

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    // Simple passthrough - just render the quad at the origin
    gl_Position = vec4(inPos * 0.1, 0.0, 1.0); // Small quad in center
    
    // Fixed color for testing
    fragColor = vec4(1.0, 0.8, 0.4, 0.8); // Golden color
    fragTexCoord = inPos * 0.5 + 0.5; // Convert from [-1,1] to [0,1]
}