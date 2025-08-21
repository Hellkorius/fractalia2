# ECS Components Directory

## Purpose
Core ECS component definitions for the Fractalia2 engine. This directory contains all data structures that define entity properties and behaviors within the Flecs ECS system, serving as the foundation for CPU-GPU data flow and service interactions.

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
- GLM math library types (vec3, vec4, mat4, quat) for transforms and colors
- Raw SDL input data (keyboard scancodes, mouse positions/buttons, wheel events)
- Frame timing information (deltaTime, frameCount) from main loop
- Movement parameters (amplitude, frequency, phase) for GPU compute shaders

**Outputs:**
- **Transform**: Position/rotation/scale data → GPU model matrices via getMatrix()
- **Renderable**: Color/visibility/layer data → GPU fragment shaders
- **MovementPattern**: Movement parameters → GPU compute shaders (movement_random.comp)
- **KeyboardInput/MouseInput**: Input states → InputService/ControlService
- **CullingData/LODData**: Optimization hints → RenderingService culling systems
- **Tag components** (Static/Dynamic/Pooled) → Entity lifecycle and filtering systems

**Key Components Data Flow:**
- **Transform**: TRS matrix with dirty flag optimization → lazy matrix computation → GPU upload
- **Renderable**: Color/layer/visibility with version tracking → batch rendering system
- **MovementPattern**: RandomWalk parameters → GPUEntity structure → compute shader uniforms
- **KeyboardInput/MouseInput**: Frame-based states → service layer action mapping
- **Lifetime**: Entity age tracking → automatic cleanup systems
- **Application/GPU sync components**: Global state coordination

### camera_component.h
**Inputs:**
- Camera movement commands (WASD movement, zoom controls, rotation)
- Screen dimensions for aspect ratio and viewport calculations
- World positions for screen-to-world coordinate transformations

**Outputs:**
- **View matrix** → Vulkan uniform buffers (GPU rendering pipeline)
- **Projection matrix** → Vulkan uniform buffers (Y-flipped for Vulkan coordinate system)
- **Frustum data** → CameraService and culling systems
- **Screen-to-world coordinates** → Input handling and mouse interaction systems

**Features:**
- Lazy matrix computation with dirty flags for performance optimization
- Unbounded zoom support for large-scale entity visualization
- Screen-to-world coordinate conversion for precise mouse interaction

### entity.h
**Inputs:**
- Flecs entity references from ECS world
- ECS component data (Transform, Renderable, MovementPattern) via has<>() and get<>()

**Outputs:**
- **EntityHandle**: Type alias for flecs::entity → service layer entity management
- **GPUEntityData**: Bridge structure for CPU→GPU synchronization via GPUEntityManager

## Peripheral Dependencies

### CPU Systems Integration (Primary Consumers)
- **Services** (`../services/`): CameraService, InputService, RenderingService, ControlService consume component data
- **Core** (`../core/`): EntityFactory uses components for entity creation patterns
- **Utilities** (`../utilities/`): ComponentQueries access component data for performance monitoring
- **Systems** (`../systems/`): MovementSystem, LifetimeSystem process component updates

### GPU Pipeline Integration (Data Transform Chain)
1. **GPUEntityManager** (`../gpu/gpu_entity_manager.*`): Converts ECS components → SoA (Structure of Arrays) buffers
2. **SoA Buffer Layout**: Separate specialized buffers for optimal GPU access
   - VelocityBuffer: Movement velocity data for compute shaders
   - MovementParamsBuffer: Movement parameters (amplitude, frequency, phase, timeOffset)
   - RuntimeStateBuffer: Entity runtime state (totalTime, stateTimer, initialization flags)
   - ColorBuffer: RGBA color data for fragment shaders
   - ModelMatrixBuffer: Transform matrices for vertex shaders
   - PositionBuffer: Current/target positions for compute-graphics pipeline
3. **Vulkan Compute Pipeline**: Processes SoA data in `movement_random.comp` and `physics.comp` shaders
4. **Vulkan Graphics Pipeline**: Renders entities using instanced drawing from specialized SoA buffers

### External Dependencies
- **Flecs ECS**: Core entity-component system framework
- **GLM**: Mathematics library for vectors, matrices, quaternions
- **SDL3**: Input event handling (keyboard/mouse state management)
- **Vulkan**: GPU rendering and compute operations target

## Data Flow Architecture

```
ECS Components → GPUEntityManager → SoA Vulkan Buffers → GPU Shaders
     ↓                ↓                      ↓               ↓
Input Systems    Service Layer          Buffer Sync      Compute/Graphics
     ↓                ↓                      ↓               ↓
Frame Logic     Culling/LOD            GPU Upload       Instanced Rendering
```

### Component Lifecycle
1. **Creation**: EntityFactory creates entities with component combinations
2. **Processing**: Services/Systems read/modify component data each frame
3. **GPU Sync**: GPUEntityManager::addEntitiesFromECS() converts components to SoA format
4. **Upload**: Staged entities uploaded to specialized GPU buffers via uploadPendingEntities()
5. **Rendering**: Vulkan pipeline consumes specialized SoA buffers for instanced draws
6. **Cleanup**: LifetimeSystem manages entity destruction based on Lifetime component

## Key Notes & Conventions

### Performance Optimizations
- **Dirty Flags**: Transform and Camera use lazy computation to avoid redundant matrix calculations
- **Version Tracking**: Renderable components track changes (markDirty()) for efficient GPU uploads
- **SoA Buffer Layout**: Structure of Arrays optimizes GPU memory access patterns and cache efficiency
- **POD Design**: All components are Plain Old Data for ECS memory efficiency

### GPU Synchronization Patterns
- **Component → SoA Conversion**: Components must be mappable to specialized SoA buffer layouts
- **Movement Parameters**: Direct mapping to compute shader uniforms (movementParams buffer)
- **Transform Matrices**: Cached via getMatrix() and uploaded to ModelMatrixBuffer for GPU
- **Color Data**: Direct vec4 passthrough to ColorBuffer for fragment shaders

### Input System Architecture
- **Frame-Based States**: Distinction between current frame (pressed/released) vs. continuous (held)
- **SDL Scancode Integration**: Raw scancodes used for keyboard input processing
- **Coordinate Systems**: Mouse coordinates tracked in both screen and world space
- **Event Buffering**: InputEvents component buffers SDL events for ECS processing

### Component Design Patterns
- **Tag Components**: Empty structs for entity filtering (Static, Dynamic, Pooled)
- **Singleton Components**: Global state (ApplicationState, GPUEntitySync, InputState)
- **Marker Components**: Processing state tracking (InputProcessed, ControlsProcessed)
- **Dirty Flag Pattern**: Transform and Camera optimize expensive computations

### Critical Integration Points
- **MovementPattern ↔ Compute Shader**: Parameters must align with movement_random.comp shader uniforms
- **Transform Matrix**: Must match GPU vertex shader matrix multiplication order
- **Input Components ↔ SDL**: Layout must align with SDL event structure for efficient processing
- **SoA Buffer Conversion**: All referenced components must remain ABI-stable for specialized GPU buffer upload

---

**⚠️ Update Requirements**: This file must be updated when:
- New components are added to any header file
- Component structure changes (especially MovementPattern/Transform matrix computation)
- SoA buffer layout modifications affecting component conversion
- Service architecture changes affecting component consumption patterns
- Input handling modifications requiring component structure updates
- Shader interface changes requiring component parameter adjustments
- GPU synchronization patterns change affecting component→SoA data flow