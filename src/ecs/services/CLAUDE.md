# ECS Services Directory - AI Agent Context

## Purpose
Service-based architecture layer providing high-level game systems with dependency injection, lifecycle management, and clean separation of concerns. Services bridge between low-level ECS components/systems and external subsystems (Vulkan, SDL3, etc).

## File/Folder Hierarchy
```
src/ecs/services/
├── camera/                       # Modularized camera subsystems
│   ├── camera_culling.h/cpp      # Frustum culling and visibility testing
│   ├── camera_manager.h/cpp      # Camera entity lifecycle management
│   ├── camera_transforms.h/cpp   # View/projection matrix calculations
│   ├── camera_transition_system.h/cpp # Smooth camera transitions and interpolation
│   └── viewport_manager.h/cpp    # Split-screen viewport support
├── camera_service.h/cpp          # Multi-camera management orchestration
├── input_service.h/cpp           # Action-based input system with context switching
├── control_service.h/cpp         # Game control logic coordination (GameControlService)
├── control_service_minimal.h     # Test service (TestControlService) - minimal implementation
└── rendering_service.h/cpp       # Render queue management with culling/batching
```

## Inputs & Outputs (by Component)

### CameraService
**Inputs:**
- `flecs::world&` - ECS world for camera entity management
- Window resize events (width/height) from external window system
- Delta time for transition updates
- Camera component data from ECS entities
- Transform/Bounds data for culling operations

**Outputs:**
- Camera entities with Camera components in ECS world
- View/Projection/ViewProjection matrices for rendering
- Viewport definitions for split-screen support
- Culling results (CullingInfo structs) with visibility/LOD data
- Camera transitions and interpolated camera states
- World/screen coordinate transformations

**Key APIs:**
- `createCamera()` → CameraID
- `getActiveCameraData()` → Camera*
- `transitionToCamera()` - smooth camera transitions with CameraTransition
- `handleWindowResize()` - viewport updates
- `update()` - transition processing and matrix updates

**Modular Components:**
- **CameraManager**: Entity lifecycle, creation/removal
- **CameraTransitionSystem**: Smooth interpolation between camera states
- **ViewportManager**: Split-screen viewport calculations
- **CameraCulling**: Frustum culling and visibility testing
- **CameraTransforms**: View/projection matrix generation

### InputService
**Inputs:**
- `flecs::world&` - ECS world for input entity management
- `SDL_Window*` - SDL window for event processing
- SDL events (keyboard, mouse, window events)
- Delta time for input state duration tracking
- Input configuration files (JSON/custom format)

**Outputs:**
- Input entity with KeyboardInput/MouseInput/InputState components
- Action state mappings (digital/analog values)
- Context-switched input bindings
- Raw input state arrays (keys, mouse buttons)
- Input callbacks and event notifications
- Window event state (resize, quit requests)

**Key APIs:**
- `processSDLEvents()` - consumes SDL event queue
- `isActionActive()`, `isActionJustPressed()` → bool
- `getActionAnalog1D/2D()` → float/glm::vec2
- `setContextActive()` - switches input context
- `getMouseWorldPosition()` → glm::vec2

### GameControlService (control_service.h/cpp)
**Inputs:**
- `flecs::world&` - ECS world reference
- `VulkanRenderer*` - renderer for graphics tests/debug
- `EntityFactory*` - entity creation operations
- Service dependencies: InputService, CameraService, RenderingService
- Delta time for cooldown management

**Outputs:**
- Control actions execution (entity creation, camera control, etc.)
- Performance statistics requests
- Graphics test execution
- Debug mode state changes
- Entity creation at specified positions
- Swarm creation with parameters (count, center, radius)

**Key APIs:**
- `processFrame()` - main control logic loop
- `handleInput()` - processes input actions
- `createEntity()`, `createSwarm()` - entity management
- `toggleDebugMode()`, `showPerformanceStats()` - debug operations

### RenderingService
**Inputs:**
- `flecs::world&` - ECS world for renderable entity queries
- `VulkanRenderer*` - Vulkan rendering subsystem
- Entity data: Transform, Renderable, Bounds components
- Camera data from CameraService for culling
- Delta time for performance timing

**Outputs:**
- RenderQueue entries sorted by priority/distance/state
- RenderBatch structures for GPU submission
- LOD calculations and visibility culling results
- Vulkan command buffer submissions via renderer
- Render statistics (draw calls, triangles, timing)
- GPU entity data synchronization

**Key APIs:**
- `buildRenderQueue()` - collects renderable entities
- `performCulling()` - frustum/occlusion culling
- `submitRenderQueue()` - submits to GPU via VulkanRenderer
- `calculateLOD()` → int (LOD level)

### TestControlService (control_service_minimal.h)
**Inputs:**
- None (minimal test service)

**Outputs:**  
- None (minimal test service)

**Purpose:**
- Service pattern compliance testing
- Dependency injection validation

## Data Flow Between Components

### Service Integration Pattern:
1. **ServiceLocator** manages all service instances with dependency injection
2. **Initialization Order**: Services initialize by priority (higher first)
3. **Cross-Service Communication**: Direct service references via ServiceLocator
4. **ECS Integration**: Services create/query entities, read/write components

### Typical Frame Flow:
```
InputService.processFrame() 
  → processes SDL events
  → updates action states
  → notifies callbacks

GameControlService.processFrame()
  → reads InputService actions
  → executes game logic
  → requests camera/rendering changes

CameraService.update()
  → processes transitions
  → updates camera matrices
  → provides culling data

RenderingService.processFrame()
  → queries ECS for renderable entities
  → performs culling using CameraService data
  → submits render queue to VulkanRenderer
```

## Peripheral Dependencies

### Core ECS Infrastructure:
- `../core/service_locator.h` - Dependency injection, lifecycle management
- `../core/world_manager.h` - Flecs world management and modules
- `../core/entity_factory.h` - Entity creation utilities

### ECS Data:
- `../components/component.h` - Transform, Renderable, Camera, Input components

### External Subsystems:
- `../../vulkan_renderer.h` - VulkanRenderer for GPU operations
- `../gpu_entity_manager.h` - CPU→GPU data bridge
- `SDL3/SDL.h` - Input event processing, window management
- `flecs.h` - ECS world, entity, component operations
- `glm/` - 3D math operations (matrices, vectors, quaternions)

### Rendering Pipeline:
- Services → VulkanRenderer → GPU Entity Manager → Vulkan nodes
- RenderingService builds queue → VulkanRenderer submits → EntityGraphicsNode renders

## Key Notes

### Service Architecture Conventions:
- **DECLARE_SERVICE(ServiceName)** macro for service pattern compliance
- **Priority-based initialization**: Higher priority services initialize first
- **Dependency validation**: Services declare dependencies, validated at startup  
- **Lifecycle management**: UNINITIALIZED → INITIALIZING → INITIALIZED → SHUTTING_DOWN → SHUTDOWN
- **Convenience macro**: SERVICE(ServiceClass) for streamlined access

### Critical Integration Points:
- **InputService** requires SDL_Window* for event processing
- **GameControlService** requires VulkanRenderer* and EntityFactory* for operations
- **RenderingService** requires VulkanRenderer* for GPU submission
- **All services** require flecs::world& for ECS integration

### Performance Considerations:
- RenderingService supports multithreading (`processRenderingMT()`)
- Render queue pre-allocated for maxRenderableEntities (default: 100,000)
- LOD system with configurable distance thresholds
- Frustum/occlusion culling to reduce GPU load
- Input system uses direct state arrays for performance

### Service Communication:
- Services access each other via ServiceLocator dependency injection
- No direct coupling - services declare dependencies explicitly
- Frame-coherent execution - services process in defined order
- State sharing through ECS components where appropriate

### Debug/Monitoring Features:
- All services provide statistics and debug output
- Performance monitoring integration
- Debug visualization modes (wireframe, bounds, LOD levels)
- Console output for service state inspection

---
**⚠️ Update Requirement**: This file must be updated whenever:
- New services are added to the directory
- Service APIs change (inputs/outputs)
- Dependencies are added/removed between services  
- External subsystem integrations change (VulkanRenderer, SDL3, etc.)
- ECS component/system dependencies change