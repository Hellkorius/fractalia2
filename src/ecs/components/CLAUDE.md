# ECS Components Directory

## Purpose
Core ECS component definitions for 80,000+ entity GPU-driven rendering. These components bridge the CPU (Flecs ECS) and GPU (Vulkan compute/graphics) through optimized data structures and lazy computation patterns.

## Files
- **component.h** - All component definitions (Transform, Renderable, MovementPattern, Input, etc.)
- **camera_component.h** - Camera component with 2D view control and coordinate transformations
- **entity.h** - Entity handles and GPU synchronization bridge structures

## Core Architecture Patterns

### Dirty Flag Optimization
Critical components use lazy computation to avoid expensive operations:
```cpp
// Transform matrix calculation
const glm::mat4& getMatrix() const {
    if (dirty) {
        matrix = translationMatrix * rotationMatrix * scaleMatrix;
        dirty = false;
    }
    return matrix;
}
```

### GPU Data Bridge
Components map directly to GPU structures for efficient CPU→GPU transfer:
- **Transform** → GPU model matrices via getMatrix()
- **MovementPattern** → Compute shader uniforms (amplitude, frequency, phase, timeOffset)
- **Renderable** → Fragment shader color/visibility data

## Essential Components

### Transform
- **Purpose**: TRS (Translation/Rotation/Scale) with cached matrix computation
- **GPU Flow**: getMatrix() → GPU uniform buffers → vertex shaders
- **Pattern**: Dirty flag optimization for expensive matrix calculations

### MovementPattern
- **Purpose**: Random walk parameters for GPU compute shaders
- **GPU Flow**: Parameters → compute shader uniforms → position calculations
- **Critical**: Must align with movement_random.comp shader interface

### Renderable
- **Purpose**: Color, visibility, layer data for rendering pipeline
- **GPU Flow**: Color → fragment shaders, layer → depth sorting
- **Pattern**: Version tracking with markDirty() for efficient GPU uploads

### Camera
- **Purpose**: 2D view control with unbounded zoom support
- **GPU Flow**: View/projection matrices → GPU uniform buffers
- **Features**: Screen-to-world coordinate conversion, frustum visibility checks

### Input Components (KeyboardInput, MouseInput, InputEvents)
- **Purpose**: Frame-based input state management
- **Pattern**: Pressed/released states cleared each frame, held states persist
- **Integration**: SDL events → ECS components → Service layer

## Data Flow
```
ECS Components → GPUEntityManager (SoA conversion) → GPU Buffers → Shaders
     ↓                    ↓                           ↓            ↓
Input/Logic         Service Layer               Buffer Upload   Rendering
```

## Key Integrations

### GPU Synchronization
- **GPUEntityData**: Bridge structure for ECS→GPU entity conversion
- **SoA Buffers**: Components converted to Structure-of-Arrays for GPU optimization
- **Upload Pattern**: Staged → GPU buffer upload via specialized managers

### Service Architecture
- **InputService**: Consumes Input components for action mapping
- **CameraService**: Manages Camera components with smooth transitions
- **RenderingService**: Processes Renderable/CullingData for batched rendering
- **ControlService**: Coordinates component updates across services

## Critical Conventions

### Performance
- All components are POD (Plain Old Data) for ECS memory efficiency
- Lazy computation (dirty flags) for Transform and Camera matrices
- Version tracking for change detection and selective GPU uploads

### GPU Compatibility
- MovementPattern parameters must match compute shader uniforms exactly
- Transform matrices use correct multiplication order for GPU shaders
- Color data as vec4 for direct fragment shader passthrough

### Input Processing
- Frame-based distinction: pressed/released (single frame) vs held (continuous)
- SDL scancode compatibility for keyboard processing
- World/screen coordinate dual tracking for mouse interaction

---
**Update Triggers**: Component structure changes, GPU shader interface modifications, service integration patterns, or performance optimization requirements.