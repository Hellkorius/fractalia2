#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Shared entity buffer indices for descriptor indexing
const uint VELOCITY_BUFFER = 0u;           
const uint MOVEMENT_PARAMS_BUFFER = 1u;    
const uint MOVEMENT_CENTERS_BUFFER = 2u;   
const uint RUNTIME_STATE_BUFFER = 3u;      
const uint ROTATION_STATE_BUFFER = 4u;     
const uint COLOR_BUFFER = 5u;              
const uint MODEL_MATRIX_BUFFER = 6u;       
const uint POSITION_OUTPUT_BUFFER = 7u;    
const uint CURRENT_POSITION_BUFFER = 8u;   
const uint SPATIAL_MAP_BUFFER = 9u;

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

// Vulkan 1.3 descriptor indexing - single array of all entity buffers
layout(std430, binding = 1) readonly buffer EntityBuffers {
    vec4 data[];
} entityBuffers[];


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
    // Read computed positions from physics shader output (now supporting 3D)
    vec3 worldPos = entityBuffers[POSITION_OUTPUT_BUFFER].data[gl_InstanceIndex].xyz;
    
    // Extract movement parameters for color calculation from SoA buffers
    vec4 entityMovementParams = entityBuffers[MOVEMENT_PARAMS_BUFFER].data[gl_InstanceIndex];
    float phase = entityMovementParams.z;
    float timeOffset = entityMovementParams.w;
    float entityTime = pc.time + timeOffset;
    
    // Calculate dynamic color based on movement parameters with strong per-entity individualization
    // Use instance index to create much stronger base color variation per entity
    float entityBaseHue = mod(float(gl_InstanceIndex) * 0.618034, 1.0); // Golden ratio for good distribution
    
    // Per-entity individualized timing and frequencies - INTENSE VERSION
    float entityFreqMultiplier = 0.3 + mod(float(gl_InstanceIndex) * 0.7321, 1.0) * 2.7; // Range: 0.3 to 3.0 (much wider)
    float entityPhaseOffset = mod(float(gl_InstanceIndex) * 2.3941, 6.28318530718); // Unique phase offset
    float entityTimeOffset = mod(float(gl_InstanceIndex) * 1.4142, 15.0); // Unique time offset (longer spread)
    
    // INTENSE individualized phase system - shorter, more frequent phases
    float individualTime = entityTime * entityFreqMultiplier + entityTimeOffset + phase;
    float phaseLengthVariation = 0.8 + mod(float(gl_InstanceIndex) * 0.8660, 1.0) * 1.7; // Phase length: 0.8-2.5 seconds (much faster)
    float colorPhaseTime = individualTime * 0.8 + entityPhaseOffset; // 2x faster base rate
    float colorPhase = floor(colorPhaseTime / phaseLengthVariation);
    float phaseProgress = mod(colorPhaseTime, phaseLengthVariation) / phaseLengthVariation;
    
    // More dramatic transition curves for intensity
    float phaseTransition = smoothstep(0.1, 0.9, phaseProgress); // Steeper transitions
    
    // INTENSE individualized phase-based hue shifts - much larger jumps
    float entityHueShiftAmount = 0.2 + mod(float(gl_InstanceIndex) * 0.5257, 1.0) * 0.6; // Shift amount: 20-80% (massive jumps)
    float phaseHueShift = mod(colorPhase * entityHueShiftAmount, 1.0);
    float nextPhaseHueShift = mod((colorPhase + 1.0) * entityHueShiftAmount, 1.0);
    float currentHueShift = mix(phaseHueShift, nextPhaseHueShift, phaseTransition);
    
    // Combine base hue with INTENSE individualized phase shifting
    float hue = mod(entityBaseHue + currentHueShift, 1.0);
    
    // EXTREME brightness variation with intense breathing patterns
    float entityBrightnessBase = mod(float(gl_InstanceIndex) * 0.381966, 1.0);
    float brightnessFreq = 0.4 + mod(float(gl_InstanceIndex) * 0.9511, 1.0) * 1.2; // Range: 0.4 to 1.6 (much faster)
    float brightnessPhase = sin(individualTime * brightnessFreq + entityPhaseOffset * 2.0) * 0.7; // Much stronger amplitude
    float brightness = 0.2 + entityBrightnessBase * 0.7 + brightnessPhase; // Range: -0.5 to 1.6
    brightness = clamp(brightness, 0.05, 1.0); // Allow very dim to very bright
    
    // EXTREME saturation variation with intense cycling patterns
    float entitySaturationBase = mod(float(gl_InstanceIndex) * 0.236068, 1.0);
    float saturationFreq = 0.3 + mod(float(gl_InstanceIndex) * 0.4472, 1.0) * 1.0; // Range: 0.3 to 1.3 (much faster)
    float saturationPhase = cos(individualTime * saturationFreq + entityPhaseOffset * 1.7) * 0.8; // Much stronger amplitude
    float saturation = 0.1 + entitySaturationBase * 0.8 + saturationPhase; // Range: -0.7 to 1.7
    saturation = clamp(saturation, 0.0, 1.0); // Allow completely desaturated to fully saturated
    
    // Read rotation from dedicated rotation buffer
    float rot = entityBuffers[ROTATION_STATE_BUFFER].data[gl_InstanceIndex].x;
    
    // Read base entity color from GPU entity data
    vec3 entityBaseColor = entityBuffers[COLOR_BUFFER].data[gl_InstanceIndex].rgb;
    
    // Generate dynamic color and blend with entity base color
    vec3 dynamicColor = hsv2rgb(hue, saturation, brightness);
    
    // Combine entity color with dynamic effects (weighted blend)
    color = mix(entityBaseColor, dynamicColor, 0.7); // 70% dynamic, 30% base entity color
    
    // ROTATE THE ACTUAL MESH VERTICES (3D cube geometry)
    float cosRot = cos(rot);
    float sinRot = sin(rot);
    
    // For 3D, rotate vertices in the XY plane, preserving Z
    vec3 rotatedVertex = vec3(
        inPos.x * cosRot - inPos.y * sinRot,
        inPos.x * sinRot + inPos.y * cosRot,
        inPos.z  // Preserve Z component for 3D geometry
    );
    
    // Add rotated vertex to world position (full 3D support)
    vec3 finalPos = worldPos + rotatedVertex;
    gl_Position = ubo.proj * ubo.view * vec4(finalPos, 1.0);
}