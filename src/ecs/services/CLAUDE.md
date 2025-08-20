# ECS Services Directory - AI Agent Context

## Purpose
High-level game services implementing service-based architecture with dependency injection for input management, camera control, rendering coordination, and game logic. These services bridge between the ECS world and external systems (SDL, Vulkan, etc.) with priority-based initialization and lifecycle management.

## File/Folder Hierarchy
```
src/ecs/services/
├── camera_service.h/cpp         # Multi-camera management, transitions, viewports, culling
├── input_service.h/cpp          # Action-based input with SDL integration
├── rendering_service.h/cpp      # ECS-Vulkan bridge, render queue, culling, LOD
├── control_service.h/cpp        # Game control logic and service coordination
├── camera/                      # Camera subsystem modules
│   ├── camera_manager.h/cpp          # Camera entity lifecycle and management
│   ├── camera_culling.h/cpp          # Frustum culling and LOD calculations
│   ├── camera_transforms.h/cpp       # World/screen coordinate transformations
│   ├── camera_transition_system.h/cpp # Smooth camera transitions with easing
│   ├── viewport_manager.h/cpp        # Multi-viewport management for split-screen
│   └── camera.md                     # Camera subsystem documentation
└── input/                       # Input subsystem modules
    ├── input_action_system.h/cpp     # Action binding, mapping, callbacks
    ├── input_event_processor.h/cpp   # SDL event handling, raw input state
    ├── input_context_manager.h/cpp   # Context switching, priority management
    ├── input_config_manager.h/cpp    # Configuration loading/saving, defaults
    ├── input_ecs_bridge.h/cpp        # ECS component synchronization
    ├── input_types.h                 # Core input type definitions and enums
    └── Input.md                      # Input subsystem documentation
```

## Inputs & Outputs (by Service)

### CameraService
**Inputs:**
- `flecs::world&` - ECS world for camera entity management
- Window resize events (width, height) for aspect ratio updates
- Camera creation requests (name, position, zoom, rotation)
- Transition requests (target camera, easing type, duration)
- Viewport definitions (name, camera assignment, screen regions)
- Entity transform/bounds arrays for culling operations

**Outputs:**
- `CameraID` - Unique camera identifiers for management
- `glm::mat4` - View, projection, view-projection matrices for rendering
- `std::vector<CullingInfo>` - Frustum culling results with visibility/LOD data
- `glm::vec2` - Coordinate transformations (world↔screen, viewport↔world)
- Camera state changes through smooth transitions
- Active camera tracking and viewport management

**Key APIs:**
- `createCamera()` → CameraID, `getCamera()` → Camera*
- `setActiveCamera()`, `transitionToCamera()` - camera management
- `performFrustumCulling()` → vector<CullingInfo> - batch culling
- `worldToScreen()`, `screenToWorld()` → glm::vec2 - coordinate conversion
- `createViewport()`, `getActiveViewports()` - multi-view support

### InputService
**Inputs:**
- `SDL_Event` structs from SDL event queue
- `SDL_Window*` for window-relative input queries
- `flecs::world&` for ECS component synchronization
- Input configuration files (JSON/custom format)
- Action definitions and key bindings
- Context switch requests and priority management

**Outputs:**
- `InputActionState` - Action states (digital/analog, just pressed/released)
- Raw input queries (key/mouse states, positions, deltas)
- `KeyboardInput`, `MouseInput`, `InputState` ECS components
- Action callback execution for registered handlers
- Context-filtered input binding resolution
- Window event notifications (resize, quit)

**Key APIs:**
- `isActionActive()`, `isActionJustPressed()` → bool - action queries
- `getActionAnalog1D/2D()` → float/glm::vec2 - analog input
- `getMousePosition()`, `getMouseWorldPosition()` → glm::vec2
- `registerAction()`, `bindAction()` - action configuration
- `setContextActive()`, `pushContext()` - context management

### RenderingService
**Inputs:**
- `flecs::world&` - ECS world with renderable entities
- `VulkanRenderer*` - Vulkan renderer instance for GPU submission
- Entity queries (Transform, Renderable, Bounds components)
- Camera culling results from CameraService
- `LODConfig` - Level-of-detail configuration
- Render priority assignments and batching parameters

**Outputs:**
- `std::vector<RenderQueueEntry>` - Sorted render queue with culling/LOD data
- `std::vector<RenderBatch>` - Optimized render batches for GPU submission
- `CullingStats`, `RenderStats` - Performance monitoring data
- ECS system registrations for rendering phases
- GPU data synchronization through GPUEntityManager
- Frame coordination signals and render state management

**Key APIs:**
- `buildRenderQueue()`, `sortRenderQueue()` - render preparation
- `performCulling()`, `calculateLOD()` - visibility optimization
- `createRenderBatches()`, `submitRenderQueue()` - GPU submission
- `registerRenderableEntity()` - entity management
- `setLODConfig()`, `setFrustumCullingEnabled()` - configuration

### GameControlService (Control Service)
**Inputs:**
- `flecs::world&` - ECS world for entity operations
- `VulkanRenderer*`, `EntityFactory*` - external system references
- User input actions from InputService
- Frame delta time for timing-based operations
- Control action definitions and cooldown parameters

**Outputs:**
- `ControlState` - Game control state with request flags
- Entity creation/swarm generation requests
- Camera control commands (movement, transitions, focus)
- Performance monitoring toggles and debug mode changes
- Graphics test execution and wireframe mode toggles
- Control action execution with cooldown management

**Key APIs:**
- `processFrame()`, `handleInput()` - frame processing
- `createEntity()`, `createSwarm()` - entity generation
- `toggleMovementType()`, `showPerformanceStats()` - game actions
- `handleCameraControls()`, `resetCamera()` - camera integration
- `registerAction()`, `executeAction()` - action management


## Data Flow Between Services

### Service Initialization Order (by Priority):
1. **WorldManager** (Priority: 100) - ECS world and module loading
2. **InputService** (Priority: 90) - SDL event processing setup
3. **CameraService** (Priority: 80) - Camera management initialization
4. **RenderingService** (Priority: 70) - Render queue and GPU bridge setup
5. **GameControlService** (Priority: 60) - Game logic and control integration

### Frame Processing Flow:
```
GameControlService.processFrame()
  → InputService.processFrame() (SDL events, action states)
  → CameraService.update() (transitions, active camera)
  → RenderingService.processFrame()
    → buildRenderQueue() (collect entities)
    → performCulling() (frustum + occlusion using CameraService)
    → calculateLOD() (level-of-detail optimization)
    → createRenderBatches() (GPU-optimized batches)
    → submitRenderQueue() (to VulkanRenderer)
  → Service.cleanup() (frame completion)
```

### Inter-Service Communication:
- **InputService** → **CameraService**: Mouse world coordinates, viewport hit testing
- **CameraService** → **RenderingService**: View matrices, culling results, active camera
- **RenderingService** → **GPUEntityManager**: Render queue, culling data, entity transforms
- **GameControlService** → All services: Control commands, state changes, debug toggles
- **ServiceLocator**: Dependency injection, lifecycle management, priority ordering

## Peripheral Dependencies

### Core ECS Infrastructure:
- `../core/service_locator.h` - Service pattern, dependency injection, lifecycle management
- `../core/world_manager.h` - ECS world management and Flecs module loading
- `../core/entity_factory.h` - Entity creation and component assignment
- `../components/component.h` - Transform, Renderable, Bounds, Camera components

### External System Integration:
- `VulkanRenderer` - GPU rendering pipeline and Vulkan API abstraction
- `GPUEntityManager` - CPU→GPU data bridge for entity transforms/states
- `SDL3/SDL.h` - Window management, event processing, input handling
- `flecs.h` - ECS world, entity management, component queries, system registration

### Math and Utilities:
- `glm/` - Vector math, matrix operations, coordinate transformations
- Standard containers (vector, unordered_map, queue) for state management
- `<functional>` - Callbacks and action handlers
- `<mutex>` - Thread-safe service access

### GPU Pipeline Integration:
- Services coordinate CPU-side ECS data with GPU compute/graphics shaders
- RenderingService provides culling results to optimize GPU workload
- CameraService matrices flow directly to vertex shaders for world→clip transformation
- Entity transform data synchronized through GPUEntityManager buffer updates

## Key Notes

### Service Architecture Pattern:
- **DECLARE_SERVICE** macro compliance for standardized service interface
- **Priority-based initialization**: Higher priority services initialize first
- **Dependency injection**: Services access dependencies through ServiceLocator
- **Lifecycle tracking**: UNINITIALIZED → INITIALIZING → INITIALIZED → SHUTTING_DOWN → SHUTDOWN
- **Frame-coherent execution**: All services update once per frame in priority order

### Performance Optimizations:
- **Cached service references**: Avoid repeated ServiceLocator lookups
- **Batch processing**: CameraService culling operates on entity arrays
- **GPU-optimized render batches**: RenderingService minimizes draw calls
- **Early frame termination**: Skip rendering when no entities are visible
- **LOD system**: Distance-based level-of-detail reduces GPU workload

### Multi-Threading Considerations:
- **Service access is thread-safe** through ServiceLocator mutex protection
- **Frame processing is single-threaded** for data coherency
- **GPU operations are asynchronous** with proper synchronization
- **Input processing may occur on separate thread** with state synchronization

### Memory Management:
- **RAII service lifecycle**: Automatic cleanup in destruction order
- **Shared pointers for service instances**: Reference counting for safety
- **Pre-allocated containers**: Render queues and input arrays avoid allocations
- **Component pooling**: ECS entities reuse components for efficiency

### Integration Patterns:
- **Services delegate to submodules**: CameraService → camera/*, InputService → input/*
- **Bidirectional service communication**: Services can query each other's state
- **Event-driven updates**: Window resize, input events trigger service updates
- **State change notification**: Services notify dependent systems of changes

### Edge Cases & Gotchas:
- **Service initialization failure**: Returns false from initialize(), others may depend on it
- **Circular dependencies**: ServiceLocator validation prevents dependency loops
- **Frame timing consistency**: All services must complete within frame budget
- **GPU synchronization**: RenderingService must wait for previous frame completion
- **Context switching**: InputService context changes affect active key bindings
- **Camera transition interruption**: New transitions override active ones in CameraService

---
**⚠️ Update Requirement**: This file must be updated whenever:
- New services are added to the directory
- Service APIs change (inputs/outputs, public interface methods)
- Inter-service communication patterns change
- Service initialization order or priorities change
- Integration with external systems (SDL, Vulkan, ECS) changes
- Service lifecycle or dependency management changes
- Performance optimization strategies change
- New subsystem modules are added to camera/ or input/ directories