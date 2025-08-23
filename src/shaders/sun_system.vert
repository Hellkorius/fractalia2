#version 450

// Sun system vertex shader - renders both sun disc and particles

// Vertex attributes
layout(location = 0) in vec2 inPos;        // Quad position for sun disc OR particle quad

// Sun particle data structure (matches compute shader)
struct SunParticle {
    vec4 position;      // xyz = world position, w = life (0.0-1.0)
    vec4 velocity;      // xyz = velocity, w = brightness
    vec4 color;         // rgba = particle color with alpha
    vec4 properties;    // x = size, y = age, z = type, w = spawn_timer
};

// Instance data from particle buffer
layout(std430, binding = 1) readonly buffer ParticleBuffer {
    SunParticle particles[];
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
    int renderMode;  // 0 = sun disc, 1 = particles
    int instanceId;  // For particles: which particle instance
} pc;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragDepth;

void main() {
    if (pc.renderMode == 0) {
        // Render sun disc
        vec3 sunPos = ubo.sunPosition.xyz;
        float sunRadius = ubo.sunPosition.w;
        
        // Create billboard quad facing camera
        vec3 cameraPos = ubo.cameraPos.xyz;
        vec3 toCamera = normalize(cameraPos - sunPos);
        vec3 right = normalize(cross(toCamera, vec3(0, 1, 0)));
        vec3 up = cross(right, toCamera);
        
        // Scale quad by sun radius
        vec3 worldPos = sunPos + (right * inPos.x + up * inPos.y) * sunRadius;
        
        // Transform to screen space
        vec4 viewPos = ubo.viewMatrix * vec4(worldPos, 1.0);
        gl_Position = ubo.projMatrix * viewPos;
        
        // Sun color with bright intensity
        fragColor = vec4(ubo.sunColor.rgb * ubo.sunColor.a, 1.0);
        fragTexCoord = inPos * 0.5 + 0.5; // Convert from [-1,1] to [0,1]
        fragWorldPos = worldPos;
        fragDepth = length(cameraPos - worldPos);
        
    } else {
        // Render particle
        uint particleIndex = uint(pc.instanceId);
        
        // Skip dead particles
        if (particles[particleIndex].position.w <= 0.0) {
            gl_Position = vec4(-2.0, -2.0, -2.0, 1.0); // Off-screen
            fragColor = vec4(0.0);
            return;
        }
        
        vec3 particlePos = particles[particleIndex].position.xyz;
        float particleSize = particles[particleIndex].properties.x;
        vec4 particleColor = particles[particleIndex].color;
        float brightness = particles[particleIndex].velocity.w;
        
        // Create billboard quad facing camera
        vec3 cameraPos = ubo.cameraPos.xyz;
        vec3 toCamera = normalize(cameraPos - particlePos);
        vec3 right = normalize(cross(toCamera, vec3(0, 1, 0)));
        vec3 up = cross(right, toCamera);
        
        // Scale quad by particle size
        vec3 worldPos = particlePos + (right * inPos.x + up * inPos.y) * particleSize;
        
        // Transform to screen space
        vec4 viewPos = ubo.viewMatrix * vec4(worldPos, 1.0);
        gl_Position = ubo.projMatrix * viewPos;
        
        // Particle color with brightness modulation
        fragColor = vec4(particleColor.rgb * brightness * ubo.lightParams.z, particleColor.a);
        fragTexCoord = inPos * 0.5 + 0.5;
        fragWorldPos = worldPos;
        fragDepth = length(cameraPos - worldPos);
    }
}