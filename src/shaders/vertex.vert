#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Push constants
layout(push_constant) uniform PushConstants {
    float totalTime;        // Current simulation time
    float deltaTime;        // Time per frame
    float predictionTime;   // Unused - kept for compatibility
    uint entityCount;       // Total number of entities
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// GPU Entity instance data
layout(location = 2) in vec4 instanceMatrix0;    // modelMatrix row 0 (unused)
layout(location = 3) in vec4 instanceMatrix1;    // modelMatrix row 1 (unused) 
layout(location = 4) in vec4 instanceMatrix2;    // modelMatrix row 2 (unused)
layout(location = 5) in vec4 instanceMatrix3;    // modelMatrix row 3 (unused)
layout(location = 6) in vec4 instanceColor;      // color (unused)
layout(location = 7) in vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 movementParams1;    // center.xyz, movementType
layout(location = 9) in vec4 runtimeState;       // totalTime, initialized, stateTimer, entityState

layout(location = 0) out vec3 fragColor;

// Movement pattern implementations
#define MOVEMENT_PETAL 0
#define MOVEMENT_ORBIT 1
#define MOVEMENT_WAVE 2
#define MOVEMENT_TRIANGLE_FORMATION 3

// Entity states
#define STATE_SPAWN_LERP 0.0
#define STATE_NORMAL 1.0
#define STATE_ANGEL_TRANSITION 2.0
#define STATE_ORGANIC_TRANSITION 3.0

// Transition system constants
#define TRANSITION_DURATION 2.0
#define ORGANIC_TRANSITION_DURATION 1.0
#define SPAWN_LERP_DURATION 0.5

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

// Smooth transition system - entities move to origin, then to new center (Angel Mode)
vec3 computeTransitionMovement(vec3 currentPos, vec3 targetCenter, float transitionTime, float transitionPhase) {
    float halfDuration = TRANSITION_DURATION * 0.5;
    
    if (transitionPhase < 0.5) {
        // Phase 0: Move from current position to origin (0,0)
        float t = clamp(transitionTime / halfDuration, 0.0, 1.0);
        // Smooth easing function (ease-in-out)
        t = t * t * (3.0 - 2.0 * t);
        return mix(currentPos, vec3(0.0, 0.0, 0.0), t);
    } else {
        // Phase 1: Move from origin to target center
        float t = clamp((transitionTime - halfDuration) / halfDuration, 0.0, 1.0);
        // Smooth easing function (ease-in-out)
        t = t * t * (3.0 - 2.0 * t);
        return mix(vec3(0.0, 0.0, 0.0), targetCenter, t);
    }
}

// Organic transition system - entities move directly to their target position in the new pattern
vec3 computeOrganicTransition(vec3 currentPos, vec3 targetPos, float transitionTime) {
    float t = clamp(transitionTime / ORGANIC_TRANSITION_DURATION, 0.0, 1.0);
    // Smooth easing function (ease-in-out)
    t = t * t * (3.0 - 2.0 * t);
    return mix(currentPos, targetPos, t);
}

void main() {
    // Read movement parameters
    float amplitude = movementParams0.x;
    float frequency = movementParams0.y;
    float phase = movementParams0.z;
    float timeOffset = movementParams0.w;
    vec3 center = movementParams1.xyz;
    uint movementType = uint(movementParams1.w);
    
    // Extract runtime state
    float totalTime = runtimeState.x;
    bool initialized = runtimeState.y > 0.5;
    float stateTimer = runtimeState.z;
    float entityState = runtimeState.w;
    
    // Calculate current time - use pc.totalTime instead of relying on runtimeState.totalTime
    float currentTime = pc.totalTime + timeOffset;
    
    vec3 finalPos;
    
    // For now, simplify to just normal movement to get basic functionality working
    // TODO: Re-enable state management once basic movement is working
    if (true) { // Always use normal movement for now
        // Normal movement pattern computation
        switch (movementType) {
            case MOVEMENT_PETAL:
                finalPos = computePetalMovement(center, amplitude, frequency, phase, currentTime);
                break;
            case MOVEMENT_ORBIT:
                finalPos = computeOrbitMovement(center, amplitude, frequency, phase, currentTime);
                break;
            case MOVEMENT_WAVE:
                finalPos = computeWaveMovement(center, amplitude, frequency, phase, currentTime);
                break;
            case MOVEMENT_TRIANGLE_FORMATION:
                finalPos = computeTriangleFormation(center, amplitude, frequency, phase, currentTime);
                break;
            default:
                finalPos = computePetalMovement(center, amplitude, frequency, phase, currentTime);
                break;
        }
    }
    
    // Calculate rotation and color
    float rotationSpeed = amplitude * 0.1;
    float currentRotation = currentTime * rotationSpeed;
    
    float hue = mod(phase + currentTime * 0.5, 6.28318530718) / 6.28318530718;
    float saturation = 0.7 + 0.3 * sin(frequency * currentTime);
    float brightness = 0.8 + 0.2 * sin(amplitude * currentTime * 0.5);
    
    // HSV to RGB conversion
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
    
    // Build final transform matrix
    float cosR = cos(currentRotation);
    float sinR = sin(currentRotation);
    
    mat4 transformMatrix = mat4(
        cosR, sinR, 0.0, 0.0,
        -sinR, cosR, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        finalPos.x, finalPos.y, finalPos.z, 1.0
    );
    
    gl_Position = ubo.proj * ubo.view * transformMatrix * vec4(inPosition, 1.0);
    fragColor = (rgb + m);
}