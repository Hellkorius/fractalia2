#version 450

layout(location = 0) in vec3 color;
layout(location = 1) in vec3 worldPos;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec4 outColor;

void main() {
    // Normalize the interpolated normal
    vec3 norm = normalize(normal);
    
    // Define light properties
    vec3 lightDir = normalize(vec3(1.0, 2.0, 3.0));  // Light direction (from top-right-front)
    vec3 lightColor = vec3(1.0, 0.95, 0.9);         // Slightly warm white light
    
    // Ambient light (base illumination)
    float ambient = 0.3;
    
    // Diffuse lighting (Lambert's cosine law)
    float diffuse = max(dot(norm, lightDir), 0.0);
    
    // Combine ambient and diffuse lighting
    float lighting = ambient + diffuse * 0.7;
    
    // Apply lighting to the color
    vec3 litColor = color * lightColor * lighting;
    
    // Ensure we don't exceed maximum brightness
    litColor = min(litColor, vec3(1.0));
    
    outColor = vec4(litColor, 1.0);
}