#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Keyframe buffer for position + rotation + color data
layout(binding = 1, std430) readonly buffer KeyframeBuffer {
    float keyframes[]; // Raw float array: position(3) + rotation(1) + color(4) per keyframe
};

// Push constants for keyframe interpolation
layout(push_constant) uniform PushConstants {
    float totalTime;        // Current simulation time
    float deltaTime;        // Time per frame
    float predictionTime;   // Time the keyframes were predicted for
    uint entityCount;       // Total number of entities
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// GPU Entity instance data (matches GPUEntity struct from compute shader)
layout(location = 2) in vec4 instanceMatrix0;    // modelMatrix row 0 (for rotation/scale only)
layout(location = 3) in vec4 instanceMatrix1;    // modelMatrix row 1  
layout(location = 4) in vec4 instanceMatrix2;    // modelMatrix row 2
layout(location = 5) in vec4 instanceMatrix3;    // modelMatrix row 3 (position ignored, using keyframes)
layout(location = 6) in vec4 instanceColor;      // color
layout(location = 7) in vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 movementParams1;    // center.xyz, movementType
layout(location = 9) in vec4 runtimeState;       // totalTime, initialized, reserved, reserved

layout(location = 0) out vec3 fragColor;

// Movement pattern implementations (same as compute shader)
#define MOVEMENT_PETAL 0
#define MOVEMENT_ORBIT 1
#define MOVEMENT_WAVE 2
#define MOVEMENT_TRIANGLE_FORMATION 3

vec3 computePetalMovement(vec3 center, float amplitude, float frequency, float phase, float totalTime) {
    float radialOscillation = 0.5 + 0.5 * sin(totalTime * frequency * 0.3);
    float currentRadius = amplitude * radialOscillation;
    float petalAngle = totalTime * frequency + phase;
    
    return center + vec3(
        currentRadius * cos(petalAngle),
        currentRadius * sin(petalAngle),
        0.0
    );
}

vec3 computeOrbitMovement(vec3 center, float amplitude, float frequency, float phase, float totalTime) {
    float angle = totalTime * frequency + phase;
    return center + vec3(
        amplitude * cos(angle),
        amplitude * sin(angle),
        0.0
    );
}

vec3 computeWaveMovement(vec3 center, float amplitude, float frequency, float phase, float totalTime) {
    float wave = sin(totalTime * frequency + phase);
    return center + vec3(
        amplitude * wave,
        amplitude * cos(totalTime * frequency * 0.5 + phase),
        0.0
    );
}

vec3 computeTriangleFormation(vec3 center, float amplitude, float frequency, float phase, float totalTime) {
    float orbitRadius = amplitude * 2.0;
    float orbitAngle = totalTime * frequency + phase;
    vec3 orbitPosition = vec3(
        orbitRadius * cos(orbitAngle),
        orbitRadius * sin(orbitAngle),
        0.0
    );
    
    float trianglePhase = orbitAngle * 1.5;
    float triangleWarp = sin(trianglePhase) * 0.4;
    float warpedRadius = orbitRadius * (1.0 + triangleWarp);
    vec3 triangularOrbit = vec3(
        warpedRadius * cos(orbitAngle),
        warpedRadius * sin(orbitAngle),
        0.0
    );
    
    float cosmicTime = totalTime * frequency + phase;
    float foldingPhase = sin(cosmicTime * 0.7 + phase * 2.0);
    float dimensionalFold = pow(abs(foldingPhase), 3.0) * sign(foldingPhase);
    
    float radialOscillation = 0.5 + 0.5 * sin(totalTime * frequency * 0.3 + phase * 2.0);
    float interweavingScale = 0.8 + 0.4 * radialOscillation;
    
    float distanceFromCenter = length(triangularOrbit);
    float curvature = sin(orbitAngle * 3.0 + totalTime * frequency * 0.3) * orbitRadius * 0.1;
    vec3 curvatureVector = vec3(-sin(orbitAngle), cos(orbitAngle), 0.0) * curvature;
    
    vec3 finalPosition = triangularOrbit * interweavingScale + curvatureVector;
    finalPosition += vec3(dimensionalFold * 0.1, dimensionalFold * 0.15, 0.0);
    
    return center + finalPosition;
}

void main() {
    // Get entity ID from instance index
    uint entityID = gl_InstanceIndex;
    
    // Extract movement parameters from instance data
    float amplitude = movementParams0.x;
    float frequency = movementParams0.y;
    float phase = movementParams0.z;
    float timeOffset = movementParams0.w;
    vec3 center = movementParams1.xyz;
    uint movementType = uint(movementParams1.w);
    
    // Compute current real-time position
    float currentTime = pc.totalTime + timeOffset;
    vec3 currentPos;
    switch (movementType) {
        case MOVEMENT_PETAL:
            currentPos = computePetalMovement(center, amplitude, frequency, phase, currentTime);
            break;
        case MOVEMENT_ORBIT:
            currentPos = computeOrbitMovement(center, amplitude, frequency, phase, currentTime);
            break;
        case MOVEMENT_WAVE:
            currentPos = computeWaveMovement(center, amplitude, frequency, phase, currentTime);
            break;
        case MOVEMENT_TRIANGLE_FORMATION:
            currentPos = computeTriangleFormation(center, amplitude, frequency, phase, currentTime);
            break;
        default:
            currentPos = computePetalMovement(center, amplitude, frequency, phase, currentTime);
            break;
    }
    
    // Compute current rotation and color
    float rotationSpeed = amplitude * 0.1;
    float currentRotation = currentTime * rotationSpeed;
    
    // Generate current dynamic color
    float hue = mod(phase + currentTime * 0.5, 6.28318530718) / 6.28318530718;
    float saturation = 0.7 + 0.3 * sin(frequency * currentTime);
    float brightness = 0.8 + 0.2 * sin(amplitude * currentTime * 0.5);
    
    // Convert HSV to RGB
    float c = brightness * saturation;
    float x = c * (1.0 - abs(mod(hue * 6.0, 2.0) - 1.0));
    float m = brightness - c;
    
    vec3 rgb;
    if (hue < 1.0/6.0) rgb = vec3(c, x, 0);
    else if (hue < 2.0/6.0) rgb = vec3(x, c, 0);
    else if (hue < 3.0/6.0) rgb = vec3(0, c, x);
    else if (hue < 4.0/6.0) rgb = vec3(0, x, c);
    else if (hue < 5.0/6.0) rgb = vec3(x, 0, c);
    else rgb = vec3(c, 0, x);
    
    vec4 currentColor = vec4(rgb + m, 1.0);
    
    // Use current real-time values for perfectly smooth movement
    // The keyframe system now only provides computational savings, not rendering
    
    // Build transform matrix with current rotation and position
    float cosR = cos(currentRotation);
    float sinR = sin(currentRotation);
    
    mat4 transformMatrix = mat4(
        cosR, sinR, 0.0, 0.0,
        -sinR, cosR, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        currentPos.x, currentPos.y, currentPos.z, 1.0
    );
    
    gl_Position = ubo.proj * ubo.view * transformMatrix * vec4(inPosition, 1.0);
    fragColor = currentColor.rgb;
}