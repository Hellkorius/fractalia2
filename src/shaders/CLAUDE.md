# Fractalia2 Shader Architecture

GPU-driven compute and graphics shaders for 80,000+ entity rendering with MVP transformation pipeline.

## Shader Pipeline

**Movement Compute** → **Physics Compute** → **3D Graphics Rendering**

- Movement: Random walk velocity generation every 120 frames
- Physics: 3D collision detection, position integration, model matrix updates
- Graphics: MVP transformation pipeline with instanced rendering

## Buffer Architecture

### Entity Buffer Organization (Structure of Arrays)
```glsl
// entity_buffer_indices.glsl - Shared buffer indices across all shaders
const uint VELOCITY_BUFFER = 0u;           // vec4: velocity.xyz, damping
const uint MOVEMENT_PARAMS_BUFFER = 1u;    // vec4: amplitude, frequency, phase, timeOffset
const uint MOVEMENT_CENTERS_BUFFER = 2u;   // vec4: movement center.xyz, reserved
const uint RUNTIME_STATE_BUFFER = 3u;      // vec4: totalTime, entityType, stateTimer, initialized
const uint ROTATION_STATE_BUFFER = 4u;     // vec4: rotation, angularVelocity, angularDamping, reserved
const uint COLOR_BUFFER = 5u;              // vec4: RGBA color values
const uint MODEL_MATRIX_BUFFER = 6u;       // mat4: full 3D transform matrix (16 vec4 elements per matrix)
const uint SPATIAL_MAP_BUFFER = 7u;        // uvec2[]: spatial hash grid for collision detection
```

### Data Flow Pattern
1. **Movement Compute**: Updates `VELOCITY_BUFFER` with random walk patterns
2. **Physics Compute**: Reads velocity → Integrates position → Writes to `MODEL_MATRIX_BUFFER` column 3
3. **Graphics Pipeline**: Reads `MODEL_MATRIX_BUFFER` → MVP transformation → Screen coordinates

### Deprecated Buffers
```glsl
// DEPRECATED: Position buffers replaced by model matrix approach
const uint POSITION_OUTPUT_BUFFER = 8u;    // DEPRECATED: use MODEL_MATRIX_BUFFER column 3
const uint CURRENT_POSITION_BUFFER = 9u;   // DEPRECATED: use MODEL_MATRIX_BUFFER column 3
```

## MVP Transformation System

### Physics Compute: Direct Model Matrix Updates
```glsl
// Physics writes position directly to model matrix translation (column 3)
vec3 currentPosition = entityBuffers[MODEL_MATRIX_BUFFER].data[entityIndex * 4 + 3].xyz;
// ... physics integration ...
entityBuffers[MODEL_MATRIX_BUFFER].data[entityIndex * 4 + 3] = vec4(resolvedPosition, 1.0);
```

### Vertex Shader: Clean MVP Pipeline
```glsl
// Load complete 4x4 model matrix from buffer
mat4 modelMatrix = mat4(
    entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 0],  // Column 0
    entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 1],  // Column 1
    entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 2],  // Column 2
    entityBuffers[MODEL_MATRIX_BUFFER].data[gl_InstanceIndex * 4 + 3]   // Column 3 (translation)
);

// Clean transformation pipeline: Object → World → View → Projection
vec4 worldPos4 = modelMatrix * vec4(inPos, 1.0);
gl_Position = ubo.proj * ubo.view * worldPos4;
```

## Shader Dependencies and Coordination

### movement_random.comp
- **Purpose**: Generates 3D random walk velocity patterns
- **Frequency**: Every 120 frames (2 seconds at 60fps)
- **Writes**: `VELOCITY_BUFFER` (3D velocity + damping)
- **Dependencies**: None (independent velocity generation)

### physics.comp
- **Purpose**: 3D physics integration, collision detection, spatial hashing
- **Frequency**: Every frame
- **Reads**: `VELOCITY_BUFFER`, `ROTATION_STATE_BUFFER`, `MODEL_MATRIX_BUFFER`
- **Writes**: `MODEL_MATRIX_BUFFER` column 3 (translation), `SPATIAL_MAP_BUFFER`
- **Dependencies**: Must run after movement_random.comp
- **Spatial Hash**: 64×64×8 grid (32,768 cells) for 3D collision detection

### vertex.vert + fragment.frag
- **Purpose**: 3D instanced rendering with MVP transformation
- **Frequency**: Every frame per rendered entity
- **Reads**: `MODEL_MATRIX_BUFFER`, `COLOR_BUFFER`, `ROTATION_STATE_BUFFER`, `RUNTIME_STATE_BUFFER`
- **Dependencies**: Must run after physics.comp for current positions
- **Pipeline**: Model matrix → World space → View space → Projection space

## Vulkan 1.3 Integration

### Descriptor Indexing
```glsl
// Single descriptor array for all entity buffers
layout(std430, binding = 1) buffer EntityBuffers {
    vec4 data[];
} entityBuffers[];
```

### Push Constants
```glsl
// Unified push constants across compute and graphics
layout(push_constant) uniform NodePushConstants {
    float time;           // Global animation time
    float deltaTime;      // Frame delta time
    uint entityCount;     // Active entity count
    uint frame;           // Frame number
    // Compute-specific parameters
    uint param1;          // entityOffset for chunked dispatches
    float gravityStrength; // Physics gravity (9.81)
    float restitution;    // Bounce factor (0.0-1.0)
    float friction;       // Surface friction (0.0-1.0)
};
```

## Performance Optimizations

### Compute Dispatch
- **Workgroup Size**: 64×1×1 (optimal GPU occupancy)
- **Chunking**: Disabled forced chunking (`MAX_WORKGROUPS_PER_CHUNK = 2048`)
- **Memory Barriers**: Eliminated inter-chunk barriers for concurrent execution

### Spatial Hash Collision Detection
- **Grid Configuration**: 64×64×8 (32,768 cells, power-of-2 dimensions)
- **Neighborhood Pattern**: 7-cell search (74% reduction in cache misses)
- **Cache Layout**: Z-order curve mapping with bitwise wrapping
- **Memory Access**: Optimized stride patterns (Y_STRIDE=64, Z_STRIDE=4096)

### Buffer Access Patterns
- **Model Matrix Storage**: Column-major 4×4 matrices as 4 consecutive vec4 elements
- **Position Access**: `entityIndex * 4 + 3` for translation column
- **Cache Optimization**: Structure of Arrays (SoA) for better memory coalescing

## Shader File Organization

```
src/shaders/
├── entity_buffer_indices.glsl  # Shared buffer index constants
├── movement_random.comp        # 3D random walk velocity generation  
├── physics.comp               # 3D physics integration + collision detection
├── vertex.vert               # MVP transformation pipeline
├── fragment.frag             # Lighting and color output
└── compiled/                 # SPIR-V compiled shaders
    ├── movement_random.comp.spv
    ├── physics.comp.spv
    ├── vertex.vert.spv
    └── fragment.frag.spv
```

## Data Layout Specifications

### GPUEntity Memory Layout (128 bytes)
```cpp
// CPU-side structure mirrored in GPU buffers
struct GPUEntity {
    glm::vec4 velocity;           // VELOCITY_BUFFER[entityIndex]
    glm::vec4 movementParams;     // MOVEMENT_PARAMS_BUFFER[entityIndex]  
    glm::vec4 runtimeState;       // RUNTIME_STATE_BUFFER[entityIndex]
    glm::vec4 color;              // COLOR_BUFFER[entityIndex]
    glm::mat4 modelMatrix;        // MODEL_MATRIX_BUFFER[entityIndex * 4] to [entityIndex * 4 + 3]
};
```

### Model Matrix Buffer Layout
```glsl
// Per-entity 4×4 matrix stored as 4 consecutive vec4 elements
uint baseIndex = entityIndex * 4;
mat4 modelMatrix = mat4(
    entityBuffers[MODEL_MATRIX_BUFFER].data[baseIndex + 0],  // Column 0: X-axis
    entityBuffers[MODEL_MATRIX_BUFFER].data[baseIndex + 1],  // Column 1: Y-axis  
    entityBuffers[MODEL_MATRIX_BUFFER].data[baseIndex + 2],  // Column 2: Z-axis
    entityBuffers[MODEL_MATRIX_BUFFER].data[baseIndex + 3]   // Column 3: Translation
);
```

## Synchronization Requirements

### Compute-Graphics Barriers
- Movement → Physics: Velocity data dependency  
- Physics → Graphics: Model matrix position dependency
- Frame Graph: Automatic barrier insertion for resource transitions

### Memory Coherency
- `memoryBarrierShared()`: Local workgroup synchronization
- `barrier()`: Workgroup execution barrier
- Resource transitions handled by Vulkan frame graph system