#version 450

// Sun disc fragment shader - simple and cool
layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Brighter sun to ensure visibility
    vec3 sunColor = vec3(1.0, 0.95, 0.8) * 1.2;  // Bright enough to see
    
    outColor = vec4(sunColor, 1.0);
}