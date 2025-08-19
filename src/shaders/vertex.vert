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
    
    // Calculate dynamic color based on movement parameters with strong per-entity individualization
    // Use instance index to create much stronger base color variation per entity
    float entityBaseHue = mod(float(gl_InstanceIndex) * 0.618034, 1.0); // Golden ratio for good distribution
    
    // Per-entity individualized timing and frequencies to break synchronization
    float entityFreqMultiplier = 0.5 + mod(float(gl_InstanceIndex) * 0.7321, 1.0) * 1.5; // Range: 0.5 to 2.0
    float entityPhaseOffset = mod(float(gl_InstanceIndex) * 2.3941, 6.28318530718); // Unique phase offset
    float entityTimeOffset = mod(float(gl_InstanceIndex) * 1.4142, 10.0); // Unique time offset
    
    // Individualized phase system - each entity has different phase length and timing
    float individualTime = entityTime * entityFreqMultiplier + entityTimeOffset + phase;
    float phaseLengthVariation = 2.0 + mod(float(gl_InstanceIndex) * 0.8660, 1.0) * 3.0; // Phase length: 2-5 seconds
    float colorPhaseTime = individualTime * 0.4 + entityPhaseOffset;
    float colorPhase = floor(colorPhaseTime / phaseLengthVariation);
    float phaseProgress = mod(colorPhaseTime, phaseLengthVariation) / phaseLengthVariation;
    
    // Smooth transition using smoothstep for organic feel
    float phaseTransition = smoothstep(0.0, 1.0, phaseProgress);
    
    // Create individualized phase-based hue shifts
    float entityHueShiftAmount = 0.1 + mod(float(gl_InstanceIndex) * 0.5257, 1.0) * 0.3; // Shift amount: 10-40%
    float phaseHueShift = mod(colorPhase * entityHueShiftAmount, 1.0);
    float nextPhaseHueShift = mod((colorPhase + 1.0) * entityHueShiftAmount, 1.0);
    float currentHueShift = mix(phaseHueShift, nextPhaseHueShift, phaseTransition);
    
    // Combine base hue with individualized phase shifting
    float hue = mod(entityBaseHue + currentHueShift, 1.0);
    
    // Highly individualized brightness variation with unique breathing patterns
    float entityBrightnessBase = mod(float(gl_InstanceIndex) * 0.381966, 1.0);
    float brightnessFreq = 0.2 + mod(float(gl_InstanceIndex) * 0.9511, 1.0) * 0.4; // Range: 0.2 to 0.6
    float brightnessPhase = sin(individualTime * brightnessFreq + entityPhaseOffset * 2.0) * 0.4;
    float brightness = 0.4 + entityBrightnessBase * 0.5 + brightnessPhase;
    brightness = clamp(brightness, 0.2, 1.0);
    
    // Highly individualized saturation variation with unique cycling patterns
    float entitySaturationBase = mod(float(gl_InstanceIndex) * 0.236068, 1.0);
    float saturationFreq = 0.15 + mod(float(gl_InstanceIndex) * 0.4472, 1.0) * 0.35; // Range: 0.15 to 0.5
    float saturationPhase = cos(individualTime * saturationFreq + entityPhaseOffset * 1.7) * 0.45;
    float saturation = 0.3 + entitySaturationBase * 0.6 + saturationPhase;
    saturation = clamp(saturation, 0.1, 1.0);
    
    color = hsv2rgb(hue, saturation, brightness);
    
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