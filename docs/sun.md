# Sun Particle System

## Architecture

**Dual-Node Design**
- `SunParticleComputeNode` - Compute-only particle physics simulation
- `SunSystemNode` - Graphics-only sun disc + particle rendering
- Frame graph ensures compute executes before graphics

## Resource Flow

```
SunParticleComputeNode creates → "SunParticleBuffer" (64 particles × 64 bytes)
                                      ↓
SunSystemNode accesses → Same buffer for rendering
```

## Data Structures

**SunParticle (64 bytes)**
```cpp
struct SunParticle {
    vec4 position;    // xyz = world pos, w = life (0.0-1.0)
    vec4 velocity;    // xyz = velocity, w = brightness  
    vec4 color;       // rgba particle color
    vec4 properties;  // x = size, y = age, z = type, w = timer
};
```

**SunUBO (Shared)**
```cpp
struct SunUBO {
    mat4 viewMatrix, projMatrix;
    vec4 sunPosition;     // xyz = pos, w = radius
    vec4 sunColor;        // rgb = color, a = intensity
    vec4 cameraPos;       // xyz = pos, w = fov
    vec4 sceneInfo;       // x = time, y = deltaTime, z = count, w = wind
    vec4 lightParams;     // x = rayLength, y = rayIntensity, z = brightness, w = gravity
};
```

## Compute Shader (`sun_system.comp`)

**Dispatch**: 64 threads, 1 workgroup for 64 particles
**Physics**: Spawn → Forces (gravity, wind) → Aging → Death → Respawn
**Safety**: Division by zero protection, bounds clamping

## Graphics Shaders (`sun_system.vert/frag`)

**Dual Mode Rendering**:
- `renderMode = 0` - Sun disc (billboard quad)
- `renderMode = 1` - Particles (instanced billboards)

**Instancing**: 6 vertices × 64 instances = 384 vertices total

## Synchronization

**Race Condition Prevention**: Resources created during execution, not initialization
**Buffer Sharing**: Compute node exposes buffer ID to graphics node via direct reference
**Memory Barriers**: Frame graph handles compute→graphics synchronization automatically

## Configuration

**Default Parameters**:
- 64 particles (configurable via `setMaxParticles`)
- Sun position: `(0, 50, 0)` - high in sky
- Particle lifetime: 10 seconds
- Wind strength: 0.3, Gravity: 0.1

**Pipeline Dependencies**:
- Descriptor layout: `createSunSystemLayout()` (UBO + storage buffer)
- Compute pipeline: `createSunParticleState()` → `sun_system.comp.spv`
- Graphics pipeline: `createSunSystemRenderingState()` → `sun_system.vert/frag.spv`