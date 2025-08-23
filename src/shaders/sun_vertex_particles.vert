#version 450

// Sun system vertex shader - pure time-based particle simulation
layout(location = 0) in vec2 inPos;        // Quad position

// Static particle data - never changes after initialization
struct StaticParticle {
    vec4 spawnData;     // xyz = spawn position, w = spawn time offset
    vec4 seedData;      // xyzw = random seeds for variation
    vec4 typeData;      // x = particle type, y = size, z = lifetime, w = unused
};

// Read-only particle data (no compute shader needed!)
layout(std430, binding = 1) readonly buffer ParticleBuffer {
    StaticParticle particles[];
};

layout(binding = 0) uniform SunUBO {
    mat4 viewMatrix;
    mat4 projMatrix; 
    vec4 sunPosition;       // xyz = position, w = radius
    vec4 sunColor;          // rgb = color, a = intensity
    vec4 cameraPos;         // xyz = camera position, w = fov
    vec4 sceneInfo;         // x = time, y = deltaTime, z = particleCount, w = windStrength
    vec4 lightParams;       // x = rayLength, y = rayIntensity, z = particleBrightness, w = gravityStrength
} ubo;

// Push constants to specify what we're rendering
layout(push_constant) uniform PushConstants {
    int renderMode;  // 0 = background, 1 = sun disc, 2 = particles
    int instanceId;  // For particles: which particle instance
} pc;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragDepth;

// Simple random function
float random(float seed) {
    return fract(sin(seed * 12.9898) * 43758.5453);
}

vec3 calculateParticlePosition(uint particleIndex, float currentTime) {
    StaticParticle particle = particles[particleIndex];
    
    // Calculate particle age
    float spawnTime = particle.spawnData.w;
    float lifetime = particle.typeData.z;
    float age = mod(currentTime - spawnTime, lifetime); // Loop particles
    float ageNormalized = age / lifetime;
    
    // If too young, particle hasn't spawned yet
    if (age < 0.0) {
        return vec3(0.0, -1000.0, 0.0); // Off-screen
    }
    
    // Start from spawn position
    vec3 pos = particle.spawnData.xyz;
    
    // Calculate velocity based on seeds (deterministic)
    float seed1 = particle.seedData.x;
    float seed2 = particle.seedData.y;
    vec3 initialVel = vec3(
        (random(seed1) - 0.5) * 4.0,
        random(seed2) * 2.0 + 1.0,      // Upward
        (random(seed1 + seed2) - 0.5) * 4.0
    );
    
    // Apply physics over time (pure math, no accumulation)
    vec3 gravity = vec3(0.0, -ubo.lightParams.w, 0.0);
    vec3 wind = vec3(1.0, 0.0, 0.5) * ubo.sceneInfo.w;
    
    // Kinematic equations: pos = initialPos + vel*t + 0.5*accel*t^2
    pos += initialVel * age;
    pos += 0.5 * (gravity + wind) * age * age;
    
    return pos;
}

void main() {
    if (pc.renderMode == 0) {
        // Render sun disc
        vec3 sunPos = ubo.sunPosition.xyz;
        float sunRadius = ubo.sunPosition.w;
        
        vec3 cameraPos = ubo.cameraPos.xyz;
        vec3 toCamera = normalize(cameraPos - sunPos);
        vec3 right = normalize(cross(toCamera, vec3(0, 1, 0)));
        vec3 up = cross(right, toCamera);
        
        vec3 worldPos = sunPos + (right * inPos.x + up * inPos.y) * sunRadius;
        
        vec4 viewPos = ubo.viewMatrix * vec4(worldPos, 1.0);
        gl_Position = ubo.projMatrix * viewPos;
        
        // Use normal projected depth (don't force depth)
        // gl_Position.z already set by projection matrix
        
        fragColor = vec4(1.0, 0.0, 1.0, 1.0);  // BRIGHT MAGENTA for testing
        fragTexCoord = inPos * 0.5 + 0.5;
        fragWorldPos = worldPos;
        fragDepth = 1000.0;  // Very far depth
        
    } else {
        // Render particle with pure time-based calculation
        uint particleIndex = uint(gl_InstanceIndex);
        
        // Calculate current particle position
        vec3 particlePos = calculateParticlePosition(particleIndex, ubo.sceneInfo.x);
        
        // If off-screen, skip
        if (particlePos.y < -500.0) {
            gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
            fragColor = vec4(0.0);
            return;
        }
        
        // Get particle data
        StaticParticle particle = particles[particleIndex];
        float particleSize = particle.typeData.y;
        
        // Create billboard
        vec3 cameraPos = ubo.cameraPos.xyz;
        vec3 toCamera = normalize(cameraPos - particlePos);
        vec3 right = normalize(cross(toCamera, vec3(0, 1, 0)));
        vec3 up = cross(right, toCamera);
        
        vec3 worldPos = particlePos + (right * inPos.x + up * inPos.y) * particleSize;
        
        vec4 viewPos = ubo.viewMatrix * vec4(worldPos, 1.0);
        gl_Position = ubo.projMatrix * viewPos;
        
        // Time-based color variation
        float age = mod(ubo.sceneInfo.x - particle.spawnData.w, particle.typeData.z);
        float ageNorm = age / particle.typeData.z;
        
        vec3 color = mix(ubo.sunColor.rgb, vec3(1.0, 0.5, 0.2), ageNorm);
        fragColor = vec4(color, 1.0 - ageNorm);
        fragTexCoord = inPos * 0.5 + 0.5;
        fragWorldPos = worldPos;
        fragDepth = length(cameraPos - worldPos);
    }
}