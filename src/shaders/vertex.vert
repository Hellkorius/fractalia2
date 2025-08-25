#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

// Include shared entity buffer indices
#include "entity_buffer_indices.glsl"

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
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

// Vulkan 1.3 descriptor indexing - single array of all entity buffers
layout(std430, binding = 1) readonly buffer EntityBuffers {
    vec4 data[];
} entityBuffers[];


layout(location = 0) out vec3 color;
layout(location = 1) out vec3 worldPos;
layout(location = 2) out vec3 normal;

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
    // Load the model matrix for this entity instance (proper 4x4 matrix)
    // Physics now writes directly to model matrix translation (column 3)
    mat4 modelMatrix = mat4(
        entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 0],
        entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 1], 
        entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 2],
        entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 3]
    );
    
    // Add per-entity rotation from physics shader
    vec4 runtimeState = entityBuffers[RUNTIME_STATE_BUFFER].data[gl_InstanceIndex];
    bool isFloorEntity = abs(runtimeState.y - 1.0) < 0.1; // entityType == Floor
    
    if (!isFloorEntity) {
        float rot = entityBuffers[ROTATION_STATE_BUFFER].data[gl_InstanceIndex].x;
        if (abs(rot) > 0.001) {
            // Apply runtime rotation to the model matrix
            float cosRot = cos(rot);
            float sinRot = sin(rot);
            mat4 rotationMatrix = mat4(
                vec4(cosRot, sinRot, 0.0, 0.0),
                vec4(-sinRot, cosRot, 0.0, 0.0),
                vec4(0.0, 0.0, 1.0, 0.0),
                vec4(0.0, 0.0, 0.0, 1.0)
            );
            modelMatrix = modelMatrix * rotationMatrix;
        }
    }
    
    // Clean MVP transformation: Object → World → View → Projection
    vec4 worldPos4 = modelMatrix * vec4(inPos, 1.0);
    worldPos = worldPos4.xyz;
    
    // Transform normal to world space (using inverse transpose for non-uniform scaling)
    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    normal = normalize(normalMatrix * inNormal);
    
    // Final transformation to clip space
    gl_Position = ubo.proj * ubo.view * worldPos4;
    
    // Calculate color (simplified for now - can be enhanced later)
    vec3 entityBaseColor = entityBuffers[COLOR_BUFFER].data[gl_InstanceIndex].rgb;
    
    if (!isFloorEntity) {
        // Dynamic color calculation for regular entities
        float entityTime = pc.time + float(gl_InstanceIndex) * 0.1;
        float hue = mod(float(gl_InstanceIndex) * 0.618034 + entityTime * 0.2, 1.0);
        float saturation = 0.8 + sin(entityTime * 1.3) * 0.2;
        float brightness = 0.7 + sin(entityTime * 0.8 + float(gl_InstanceIndex)) * 0.3;
        
        vec3 dynamicColor = hsv2rgb(hue, saturation, brightness);
        color = mix(entityBaseColor, dynamicColor, 0.7);
    } else {
        // Static color for floor entities
        color = entityBaseColor;
    }
}