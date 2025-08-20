# Camera Services Directory - AI Agent Context

## Purpose
Modularized camera system providing multi-camera management, smooth transitions, viewport support, frustum culling, and coordinate transformations. Components are organized into focused modules that integrate through the main CameraService to provide unified camera functionality for the rendering pipeline.

## File/Folder Hierarchy
```
src/ecs/services/camera/
├── camera_manager.h/cpp          # Core camera creation, management, and entity lifecycle
├── camera_culling.h/cpp          # Frustum culling, visibility tests, and LOD calculations
├── camera_transforms.h/cpp       # World/screen coordinate transformations and matrix operations
├── camera_transition_system.h/cpp # Smooth camera transitions with easing functions
└── viewport_manager.h/cpp        # Multi-viewport management for split-screen support
```

## Inputs & Outputs (by Component)

### CameraManager
**Inputs:**
- `flecs::world&` - ECS world for camera entity management
- `Camera` structs - Camera configuration data (position, zoom, rotation, viewSize)
- `std::string` - Camera names for identification
- Window resize events (width/height) for aspect ratio updates
- Entity positioning requests (move, focus, zoom operations)

**Outputs:**
- `CameraID` - Unique camera identifiers (uint32_t)
- `flecs::entity` - Camera entities with Camera components in ECS world
- Camera entity management (creation, removal, lookup)
- Active camera state tracking and switching
- Camera entity lifecycle (ensureMainCamera, cleanup)
- Debug output (camera info, statistics)

**Key APIs:**
- `createCamera()` → CameraID
- `getCamera(CameraID)` → Camera*
- `setActiveCamera()`, `getActiveCameraID()` → void, CameraID
- `handleWindowResize()` - updates aspect ratios

### CameraCulling
**Inputs:**
- `std::vector<Transform>&` - Entity world positions and orientations
- `std::vector<Bounds>&` - Entity bounding boxes (min/max vectors)
- `const Camera*` - Camera configuration for frustum calculations
- Entity world positions (glm::vec3) for visibility tests
- LOD distance thresholds (configurable float array)

**Outputs:**
- `std::vector<CullingInfo>` - Visibility results with position/bounds/visible/distance/lodLevel
- `bool` - Individual entity visibility status
- `int` - LOD level calculations (0-3 default levels)
- `CameraBounds` - Camera frustum bounds (min/max, validity flag)
- Distance-to-camera calculations for sorting/optimization

**Key APIs:**
- `performFrustumCulling()` → vector<CullingInfo>
- `isEntityVisible()`, `isPositionVisible()` → bool
- `calculateLODLevel()` → int
- `setLODDistances()` - configures distance thresholds

### CameraTransforms
**Inputs:**
- `glm::vec3` - World positions for transformation
- `glm::vec2` - Screen/viewport coordinates 
- `const Camera*` - Camera state for matrix calculations
- `const Viewport*` - Viewport configuration for multi-view transforms
- Screen size dimensions (glm::vec2) for coordinate scaling

**Outputs:**
- `glm::vec2` - Converted coordinates (world↔screen, viewport↔world)
- `glm::vec3` - Camera position/rotation data extraction
- `glm::mat4` - View, projection, and combined view-projection matrices
- `float` - Camera zoom and rotation values
- Coordinate transformation results for rendering and input processing

**Key APIs:**
- `worldToScreen()`, `screenToWorld()` → glm::vec2
- `viewportToWorld()` - multi-viewport coordinate conversion
- `getViewMatrix()`, `getProjectionMatrix()` → glm::mat4
- `setScreenSize()` - updates coordinate scaling

### CameraTransitionSystem
**Inputs:**
- `float deltaTime` - Frame time for transition progress updates
- `CameraID` - Source and target camera identifiers
- `const Camera*` - Source and target camera states
- `CameraTransition` - Transition configuration (type, duration, callbacks)
- `CameraTransitionType` - Easing types (INSTANT, LINEAR, SMOOTH_STEP, EASE_IN/OUT, SPRING)

**Outputs:**
- `Camera` - Interpolated camera states during transitions
- Transition progress tracking (active/completed states)
- Easing function evaluation results (0.0-1.0 progress)
- Transition completion callbacks execution
- Smooth camera state interpolation between keyframes

**Key APIs:**
- `transitionToCamera()` - initiates camera-to-camera transitions
- `transitionActiveCameraTo()` - transitions current camera to target state
- `update()` - advances transition progress each frame
- `getCurrentTransitionState()` → Camera

### ViewportManager
**Inputs:**
- `std::string` - Viewport names for identification
- `CameraID` - Camera assignments for each viewport
- `glm::vec2` - Viewport offset and size (normalized 0.0-1.0 coordinates)
- `Viewport` - Complete viewport configuration structs
- Screen size (glm::vec2) for coordinate conversion
- Screen points (glm::vec2) for viewport hit testing

**Outputs:**
- Viewport creation and management (name-based lookup)
- `std::vector<Viewport*>` - Active viewports sorted by render order
- `glm::vec4` - Screen rectangles (pixel coordinates) for rendering
- `bool` - Point-in-viewport hit testing results
- Viewport activation/deactivation state management
- Split-screen and multi-view rendering support

**Key APIs:**
- `createViewport()` - creates named viewport regions
- `getActiveViewports()` → vector<Viewport*>
- `findViewportAtScreenPoint()` → Viewport*
- `getViewportsByRenderOrder()` - sorted for rendering

## Data Flow Between Components

### Component Integration Pattern:
1. **CameraService** aggregates all modules and provides unified API
2. **CameraManager** handles entity lifecycle and active camera tracking  
3. **CameraTransforms** provides matrix calculations for rendering
4. **CameraCulling** processes entity arrays for visibility/LOD optimization
5. **CameraTransitionSystem** interpolates between camera states smoothly
6. **ViewportManager** manages screen regions for multi-view rendering

### Typical Frame Flow:
```
CameraService.update() 
  → CameraTransitionSystem.update() (interpolates active transitions)
  → CameraManager maintains active camera state
  → CameraTransforms provides current matrices for rendering
  → CameraCulling.performFrustumCulling() (processes entity arrays)
  → ViewportManager provides screen regions for multi-view
  → Results flow to RenderingService for GPU submission
```

### Service Integration:
- **InputService** → requests coordinate transformations for mouse/input processing
- **RenderingService** → consumes culling results and view matrices for GPU rendering
- **ControlService** → triggers camera movements, transitions, and viewport changes
- **GPU Pipeline** → receives view-projection matrices and culling data for instanced rendering

## Peripheral Dependencies

### Core ECS Infrastructure:
- `../core/service_locator.h` - Service pattern integration and dependency injection
- `../components/camera_component.h` - Camera struct with position/zoom/rotation/matrices
- `../components/component.h` - Transform, Bounds components for entity data
- `flecs.h` - ECS world, entity creation, component management

### External Libraries:
- `glm/` - 3D math operations (vec2/vec3/vec4, mat4, quaternions, transformations)
- `glm/gtc/matrix_transform.hpp` - Matrix generation functions (ortho, translate, rotate)
- Standard containers (unordered_map, vector) for camera/viewport storage

### Rendering Integration:
- CameraService bridges to VulkanRenderer through RenderingService
- View-projection matrices flow to GPU shaders for vertex transformation
- Culling results optimize GPU workload by filtering invisible entities
- Viewport data drives multi-pass rendering for split-screen scenarios

### Input Integration:
- Screen-to-world coordinate conversion for mouse interaction
- World-to-screen conversion for UI positioning relative to world objects
- Viewport hit testing for multi-view input routing

## Key Notes

### Camera Component Structure:
- **Camera struct**: position (vec3), zoom (float), rotation (float), viewSize (vec2)
- **Cached matrices**: Lazy computation with dirty flags for performance
- **Movement controls**: configurable speed parameters for smooth camera controls
- **Zoom limits**: Unbounded zoom (disabled min/max) for flexible view control

### Performance Optimizations:
- **Lazy matrix computation**: Matrices calculated only when dirty flags set
- **Batch culling operations**: Process entity arrays efficiently in CameraCulling
- **Pre-allocated containers**: Viewport and camera storage avoids allocations
- **LOD system**: Configurable distance thresholds for level-of-detail optimization

### Multi-Camera Architecture:
- **CameraID-based access**: Unique identifiers for camera management
- **Active camera concept**: Single primary camera with seamless switching
- **Transition system**: Smooth interpolation between camera states
- **Viewport support**: Multiple screen regions with independent cameras

### Coordinate Systems:
- **World coordinates**: 3D world space for entity positioning
- **Screen coordinates**: 2D pixel coordinates (0,0 top-left)
- **Viewport coordinates**: Normalized coordinates (0.0-1.0) relative to viewport
- **Clip coordinates**: NDC space for GPU pipeline compatibility

### Service Architecture Integration:
- **DECLARE_SERVICE** macro compliance for service pattern
- **Service lifecycle**: initialize() with ECS world, cleanup() for resource management
- **Dependency injection**: Access other services through ServiceLocator
- **Frame-coherent execution**: update() called consistently in service processing order

### Edge Cases & Gotchas:
- **Zero zoom handling**: Prevents division by zero in coordinate transformations
- **Matrix dirty flags**: Automatic invalidation when camera properties change
- **Entity array size matching**: CameraCulling requires transforms.size() == bounds.size()
- **Viewport bounds validation**: Screen point hit testing requires valid viewport configuration
- **Transition interruption**: New transitions can override active ones

---
**⚠️ Update Requirement**: This file must be updated whenever:
- New camera modules are added to the directory
- Camera component structure changes (position/zoom/rotation/viewSize/matrices)
- Service integration APIs change (CameraService public interface)
- Coordinate transformation logic changes (world↔screen conversions)
- Culling or LOD algorithms are modified
- Viewport or transition system functionality changes
- Dependencies on ECS components or external libraries change