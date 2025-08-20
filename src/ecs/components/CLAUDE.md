# ECS Components Directory

## Purpose
Core ECS component definitions for the Fractalia2 engine. This directory contains all data structures that define entity properties and behaviors within the Flecs ECS system.

## File/Folder Hierarchy
```
components/
├── component.h          # Core component definitions (Transform, Renderable, Movement, Input, etc.)
├── camera_component.h   # Camera component for 2D view control
├── entity.h            # Entity handles and GPU bridge structures
└── CLAUDE.md           # This documentation file
```

## Inputs & Outputs

### component.h
**Inputs:**
- GLM math library types (vec3, vec4, mat4, quat)
- Raw input data from SDL (keyboard/mouse states)
- Frame timing information (deltaTime, frameCount)

**Outputs:**
- Transform: Position/rotation/scale data → GPU transform matrices
- Renderable: Color/visibility/layer data → GPU rendering pipeline
- MovementPattern: Movement parameters → GPU compute shaders (movement_random.comp)
- KeyboardInput/MouseInput: Input states → InputService/ControlService
- CullingData/LODData: Optimization hints → RenderingService
- Tag components (Static/Dynamic/Pooled) → Entity lifecycle systems

**Key Components:**
- **Transform**: TRS matrix with dirty flag optimization, lazy matrix computation
- **Renderable**: Color, layer, visibility with version tracking for GPU sync
- **MovementPattern**: RandomWalk parameters (amplitude, frequency, phase, center) for GPU compute
- **KeyboardInput/MouseInput**: Frame-based input states with helper methods
- **Lifetime**: Entity age tracking for automatic cleanup
- **Camera**: 2D orthographic view with zoom/rotation controls

### camera_component.h
**Inputs:**
- Camera movement commands (WASD, zoom, rotation)
- Screen dimensions for aspect ratio calculation
- World positions for screen-to-world coordinate conversion

**Outputs:**
- View matrix → Vulkan uniform buffers
- Projection matrix → Vulkan uniform buffers (Y-flipped for Vulkan)
- Frustum data → Culling systems
- Screen-to-world coordinate mappings → Input handling

**Features:**
- Lazy matrix computation with dirty flags
- Unbounded zoom support for large-scale visualization
- Screen-to-world coordinate conversion for mouse interaction

### entity.h
**Inputs:**
- Flecs entity references
- ECS component data (Transform, Renderable, MovementPattern)

**Outputs:**
- EntityHandle: Type alias for flecs::entity
- GPUEntityData: Bridge structure for CPU→GPU synchronization via GPUEntityManager

## Peripheral Dependencies

### CPU Systems (Consumers)
- **Services** (`../services/`): CameraService, InputService, RenderingService, ControlService
- **Systems** (`../systems/`): Core ECS systems for entity lifecycle and movement
- **Utilities** (`../utilities/`): ComponentQueries for ECS data access patterns
- **Core** (`../core/`): EntityFactory for component-based entity creation

### GPU Pipeline (Data Flow)
1. **GPUEntityManager** (`../gpu_entity_manager.*`): Converts ECS components → GPUEntity structs
2. **GPUEntity Structure**: 128-byte cache-optimized layout (64 bytes hot data + 64 bytes cold data)
3. **Vulkan Compute Pipeline**: Processes movement data in `movement_random.comp` shader
4. **Vulkan Graphics Pipeline**: Renders entities using instanced drawing

### External Dependencies
- **Flecs ECS**: Core entity-component system
- **GLM**: Mathematics library for vectors, matrices, quaternions
- **SDL3**: Input event handling (keyboard/mouse)
- **Vulkan**: GPU rendering and compute operations

## Data Flow Architecture

```
ECS Components → GPUEntityManager → Vulkan Buffers → Shaders
     ↓                ↓                    ↓
Input Systems    Services         Compute/Graphics
     ↓                ↓                    ↓
Frame Logic     Culling/LOD       GPU Rendering
```

### Component Lifecycle
1. **Creation**: EntityFactory creates entities with components
2. **Processing**: Services/Modules read/modify component data
3. **GPU Sync**: GPUEntityManager converts to GPU-optimized format
4. **Rendering**: Vulkan pipeline processes GPU entity data
5. **Cleanup**: Lifetime system manages entity destruction

## Key Notes & Conventions

### Performance Optimizations
- **Cache Locality**: Components grouped by access patterns (hot/cold data separation)
- **Dirty Flags**: Transform and Camera use lazy computation to avoid redundant calculations
- **Version Tracking**: Renderable components track changes for efficient GPU uploads
- **Memory Layout**: GPUEntity structure optimized for GPU cache lines (128 bytes = 2 cache lines)

### GPU Synchronization
- Components must be convertible to GPUEntity format via GPUEntityManager
- MovementPattern parameters directly map to compute shader uniforms
- Transform matrices cached and uploaded to GPU as model matrices
- Color data passed directly to fragment shaders

### Input Handling
- Frame-based input states (pressed/released this frame vs. held down)
- Raw SDL scancodes used for keyboard input
- Mouse coordinates include both screen and world space positions
- Input events buffered and processed by InputService

### Component Design Patterns
- **POD Structures**: All components are Plain Old Data for ECS efficiency
- **Tag Components**: Empty structs used for filtering (Static, Dynamic, Pooled)
- **Singleton Components**: Global state (ApplicationState, GPUEntitySync, InputState)
- **Marker Components**: Processing state tracking (InputProcessed, ControlsProcessed)

### Critical Dependencies
- Changes to MovementPattern must be synchronized with `movement_random.comp` shader
- Transform matrix computation must match GPU vertex shader expectations
- Input component layout must align with SDL event structure
- GPUEntity conversion requires all referenced components to remain stable

---

**⚠️ Update Requirements**: This file must be updated when:
- New components are added to any header file
- Component structure changes (especially MovementPattern/Transform)
- GPU data layout modifications in GPUEntity
- Service architecture changes affecting component usage
- Input handling modifications
- Shader interface changes requiring component updates