#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    float time;
    float deltaTime;
    uint entityCount;
    uint frame;
} ubo;

layout(location = 0) in vec3 inPos;

// Instance data from GPUEntity buffer
layout(location = 7) in vec4 ampFreqPhaseOff;  // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 centerType;       // center.xyz, movementType

// Current positions for interpolation (from compute shader)
layout(std430, binding = 2) readonly buffer CurrentPositions {
    vec4 currentPos[];
};

// Target positions for interpolation (from compute shader)  
layout(std430, binding = 3) readonly buffer TargetPositions {
    vec4 targetPos[];
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

/* ---------- Traditional Movement Functions ---------- */
vec3 petal(vec3 center, float amplitude, float frequency, float phase, float time) {
    float r = amplitude * (0.5 + 0.5 * sin(time * frequency * 0.3));
    return center + vec3(r * cos(time * frequency + phase), r * sin(time * frequency + phase), 0);
}

vec3 orbit(vec3 center, float amplitude, float frequency, float phase, float time) {
    float ang = time * frequency + phase;
    return center + vec3(amplitude * cos(ang), amplitude * sin(ang), 0);
}

vec3 wave(vec3 center, float amplitude, float frequency, float phase, float time) {
    return center + vec3(amplitude * sin(time * frequency + phase), amplitude * cos(time * frequency * 0.5 + phase), 0);
}

vec3 triangle(vec3 center, float amplitude, float frequency, float phase, float time) {
    float ang = time * frequency + phase;
    float w = amplitude * 2.0 * (1.0 + 0.3 * sin(ang * 1.5));
    return center + vec3(w * cos(ang), w * sin(ang), 0);
}

void main() {
    float movementType = centerType.w;
    vec3 center = centerType.xyz;
    float amplitude = ampFreqPhaseOff.x;
    float frequency = ampFreqPhaseOff.y;
    float phase = ampFreqPhaseOff.z;
    float timeOffset = ampFreqPhaseOff.w;
    
    vec3 worldPos;
    
    if (movementType == 4.0) {
        // Random movement: interpolate between current and target positions
        vec3 currentPosition = currentPos[gl_InstanceIndex].xyz;
        vec3 targetPosition = targetPos[gl_InstanceIndex].xyz;
        
        // Calculate cycle using same logic as compute shader
        float cycle = mod(ubo.frame + gl_InstanceIndex * 37, 600.0);
        float t = smoothstep(0.0, 1.0, cycle / 600.0);
        
        // Handle first frame initialization
        if (ubo.frame < 1.0) {
            worldPos = center;
        } else {
            worldPos = mix(currentPosition, targetPosition, t);
        }
    } else {
        // Traditional movement patterns: compute directly
        float entityTime = ubo.time + timeOffset;
        
        uint movementTypeInt = uint(movementType);
        switch (movementTypeInt) {
            case 0:
                worldPos = petal(center, amplitude, frequency, phase, entityTime);
                break;
            case 1:
                worldPos = orbit(center, amplitude, frequency, phase, entityTime);
                break;
            case 2:
                worldPos = wave(center, amplitude, frequency, phase, entityTime);
                break;
            case 3:
                worldPos = triangle(center, amplitude, frequency, phase, entityTime);
                break;
            default:
                worldPos = center;
                break;
        }
    }
    
    // Calculate color based on movement parameters
    float entityTime = ubo.time + timeOffset;
    float hue = mod(phase + entityTime * 0.5, 6.28318) / 6.28318;
    color = hsv2rgb(hue, 0.8, 0.9);
    
    // Apply rotation based on time
    float rot = entityTime * 0.1;
    mat4 rotationMatrix = mat4(
        cos(rot), -sin(rot), 0, 0,
        sin(rot),  cos(rot), 0, 0,
        0,         0,        1, 0,
        0,         0,        0, 1
    );
    
    vec4 rotatedPos = rotationMatrix * vec4(inPos, 1.0);
    vec4 worldPosition = vec4(worldPos + rotatedPos.xyz, 1.0);
    
    gl_Position = ubo.proj * ubo.view * worldPosition;
}