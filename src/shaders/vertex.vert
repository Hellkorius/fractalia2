#version 450

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PC {
    float time;
    float dt;
    uint  count;
} pc;

// Input vertex geometry
layout(location = 0) in vec3 inPos;

// Instance data from GPUEntity buffer
layout(location = 7) in vec4 ampFreqPhaseOff;  // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 centerType;       // center.xyz, movementType (always 0 for random walk)

// Pre-computed positions from compute shader (binding = 2 matches graphics descriptor binding)
layout(std430, binding = 2) readonly buffer ComputedPositions {
    vec4 computedPos[];
};


layout(location = 0) out vec3 color;

/* ---------- HSV → RGB ---------- */
vec3 hsv2rgb(float h, float s, float v) {
    float c  = v * s;
    float x  = c * (1.0 - abs(mod(h * 6.0, 2.0) - 1.0));
    float m  = v - c;
    vec3  rgb;

    if      (h < 1.0/6.0) rgb = vec3(c, x, 0);
    else if (h < 2.0/6.0) rgb = vec3(x, c, 0);
    else if (h < 3.0/6.0) rgb = vec3(0, c, x);
    else if (h < 4.0/6.0) rgb = vec3(0, x, c);
    else if (h < 5.0/6.0) rgb = vec3(x, 0, c);
    else                    rgb = vec3(c, 0, x);

    return rgb + m;
}

void main() {
    // All entities use random walk with pre-computed positions from compute shader
    vec3 worldPos = computedPos[gl_InstanceIndex].xyz;
    
    // Extract movement parameters for color calculation
    float phase = ampFreqPhaseOff.z;
    float timeOffset = ampFreqPhaseOff.w;
    float entityTime = pc.time + timeOffset;
    
    // Calculate dynamic color based on movement parameters
    float hue = mod(phase + entityTime * 0.5, 6.28318530718) / 6.28318530718; // Use full precision PI constant
    color = hsv2rgb(hue, 0.8, 0.9);
    
    // Apply rotation based on time and final transformation
    float rot = entityTime * 0.1;
    mat4 rotationMatrix = mat4(
        cos(rot),  sin(rot), 0, 0,
       -sin(rot),  cos(rot), 0, 0,
        0,         0,        1, 0,
        worldPos,             1
    );
    
    gl_Position = ubo.proj * ubo.view * rotationMatrix * vec4(inPos, 1.0);
}