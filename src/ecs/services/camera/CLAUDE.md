# Camera Service Module - AI Agent Context

## Purpose
Modular camera system providing multi-camera management, smooth transitions, viewport support, frustum culling, and coordinate transformations. Integrates with ECS architecture and service locator pattern to provide unified camera functionality for GPU-driven rendering pipeline.

## File/Folder Hierarchy
```
src/ecs/services/camera/
├── camera_manager.h/cpp          # Core camera entity lifecycle and management
├── camera_culling.h/cpp          # Frustum culling, LOD calculation, visibility tests
├── camera_transforms.h/cpp       # Coordinate transformations and matrix operations
├── camera_transition_system.h/cpp # Smooth camera state transitions with easing
├── viewport_manager.h/cpp        # Multi-viewport split-screen management
└── CLAUDE.md                     # This documentation file
```

## Inputs & Outputs (by Component)

### CameraManager
**Inputs:**
- `flecs::world&` - ECS world instance for entity lifecycle management
- `Camera` structs - Camera configuration data (position/zoom/rotation/viewSize/matrices)
- `std::string` - Optional camera names for identification and lookup
- Window resize events (width/height integers) for automatic aspect ratio adjustments
- Movement requests (position delta, focus target, zoom level)

**Outputs:**
- `CameraID` (uint32_t) - Unique identifiers for created cameras
- `flecs::entity` - ECS entities with Camera components attached
- Active camera state tracking (single primary camera concept)
- Camera entity lifecycle management (creation/removal/cleanup)
- Window size adaptation and automatic main camera creation

**Key Data Flow:**
```
initialize(world) → ensureMainCamera() → createCameraEntity()
createCamera() → nextCameraID++ → flecs::entity creation → cameras map storage
setActiveCamera() → activeCameraID update → entity component access
```

### CameraCulling  
**Inputs:**
- `std::vector<Transform>&` - Entity world positions/orientations from ECS components
- `std::vector<Bounds>&` - Entity bounding volumes (min/max vectors)
- `const Camera*` - Camera configuration for frustum calculation
- `glm::vec3` - Individual entity positions for visibility testing
- `std::vector<float>` - Configurable LOD distance thresholds

**Outputs:**
- `std::vector<CullingInfo>` - Visibility results with position/bounds/visible/distance/lodLevel
- `bool` - Per-entity visibility status for rendering pipeline
- `int` - LOD level (0-3 default) based on distance from camera
- `CameraBounds` - Camera frustum boundaries with validity flags
- Distance calculations for depth sorting and optimization

**Key Data Flow:**
```
performFrustumCulling() → batch process Transform/Bounds arrays → CullingInfo generation
isEntityVisible() → individual frustum test → bool result
calculateLODLevel() → distance calculation → LOD index mapping
```

### CameraTransforms
**Inputs:**
- `glm::vec3` - World space positions for coordinate conversion
- `glm::vec2` - Screen coordinates and viewport coordinates for inverse transforms  
- `const Camera*` - Camera state for matrix generation and calculations
- `const Viewport*` - Viewport configuration for multi-view coordinate mapping
- `glm::vec2` - Screen dimensions for coordinate scaling operations

**Outputs:**
- `glm::vec2` - Converted coordinates (world↔screen, viewport↔world transformations)
- `glm::mat4` - View, projection, and combined view-projection matrices
- `glm::vec3` - Extracted camera position/rotation data
- `float` - Camera zoom and rotation values for rendering pipeline

**Key Data Flow:**
```
worldToScreen() → camera matrices → coordinate transformation → screen pixels
screenToWorld() → inverse matrices → world coordinates
getViewProjectionMatrix() → view * projection → GPU pipeline matrix
```

### CameraTransitionSystem
**Inputs:**
- `float deltaTime` - Frame time delta for transition progress updates
- `CameraID` pairs - Source and target camera identifiers for transitions
- `const Camera*` - Source and target camera states for interpolation
- `CameraTransition` - Configuration (type/duration/callbacks/custom easing)
- `CameraTransitionType` enum - Predefined easing (INSTANT/LINEAR/SMOOTH_STEP/EASE_IN/OUT/SPRING)

**Outputs:**
- `Camera` - Interpolated camera states during active transitions
- Transition progress tracking (active/completion status)
- Easing function evaluation (0.0-1.0 normalized progress)
- Callback execution on transition completion
- Smooth camera parameter interpolation (position/zoom/rotation)

**Key Data Flow:**
```
transitionToCamera() → CameraTransition setup → update() per frame → interpolateCameras()
evaluateEasing() → curve calculation → Camera state lerp → onComplete callback
```

### ViewportManager
**Inputs:**
- `std::string` - Viewport names for identification and lookup
- `CameraID` - Camera assignments for each viewport region
- `glm::vec2` - Normalized viewport offset and size (0.0-1.0 coordinates)
- `Viewport` structs - Complete viewport configuration with render order/clear settings
- `glm::vec2` - Screen size for pixel coordinate conversion
- `glm::vec2` - Screen points for viewport hit testing

**Outputs:**
- Viewport creation/management with name-based lookup system
- `std::vector<Viewport*>` - Active viewports sorted by render order priority
- `glm::vec4` - Screen rectangles in pixel coordinates for rendering
- `bool` - Point-in-viewport hit testing results for input routing
- Multi-view rendering support with independent camera assignments

**Key Data Flow:**
```
createViewport() → normalized coordinates → viewports map storage
getActiveViewports() → filter active → sort by render order
findViewportAtScreenPoint() → hit testing → viewport selection
```

## Data Flow Between Components

### Service Integration Pattern:
1. **CameraService** aggregates all sub-modules via unique_ptr composition
2. **Service Locator** manages CameraService lifecycle and dependencies
3. **CameraManager** handles ECS entity creation and active camera tracking
4. **CameraTransforms** provides matrix calculations for GPU pipeline
5. **CameraCulling** processes entity batches for visibility optimization
6. **CameraTransitionSystem** interpolates camera states with temporal smoothing
7. **ViewportManager** manages screen regions for split-screen rendering

### Frame Update Flow:
```
CameraService::update(deltaTime)
  → CameraTransitionSystem::update() [interpolates active transitions]
  → updateActiveCamera() [applies transition results to active camera]
  → CameraTransforms provides matrices to RenderingService
  → CameraCulling::performFrustumCulling() [processes entity arrays]
  → ViewportManager provides screen regions for multi-pass rendering
  → Results consumed by VulkanRenderer GPU pipeline
```

### Cross-Service Dependencies:
- **InputService** → requests coordinate transformations for mouse/touch input processing
- **RenderingService** → consumes view-projection matrices and culling results for GPU rendering
- **ControlService** → triggers camera movements, transitions, and viewport configuration changes
- **WorldManager** → provides ECS world instance for camera entity management

## Peripheral Dependencies

### Core Service Infrastructure:
- `/src/ecs/core/service_locator.h` - Service pattern, lifecycle management, dependency injection
- `/src/ecs/services/camera_service.h` - Main service interface implementing DECLARE_SERVICE macro
- `/src/ecs/components/camera_component.h` - Camera struct with matrices, position, zoom, rotation
- `/src/ecs/components/component.h` - Transform, Bounds, and other ECS components

### External Libraries:
- `flecs.h` - ECS world management, entity creation, component access patterns
- `glm/` - 3D mathematics (vec2/vec3/vec4, mat4, quaternions, matrix transformations)
- `glm/gtc/matrix_transform.hpp` - Matrix generation functions (ortho, translate, rotate, scale)
- `std::` containers - unordered_map, vector for camera/viewport storage and lookup

### Rendering Pipeline Integration:
- VulkanRenderer consumes view-projection matrices for vertex shader uniforms
- Culling results optimize GPU workload by filtering invisible entities before upload
- Viewport data drives multi-pass rendering for split-screen scenarios
- Camera matrices integrate with GPUEntityManager for instanced rendering

## Key Notes

### Camera Component Architecture:
- **Lazy matrix computation**: View/projection matrices calculated only when dirty flags set
- **Position/zoom/rotation**: Core camera parameters with automatic matrix invalidation
- **ViewSize and aspect ratio**: Orthographic projection parameters with window resize handling
- **Unbounded zoom**: No min/max limits for flexible camera control scenarios

### Service Pattern Compliance:
- **DECLARE_SERVICE macro**: Integrates with ServiceLocator dependency injection system
- **initialize(world) lifecycle**: ECS world binding and sub-module initialization
- **cleanup() resource management**: Proper RAII cleanup and reset of unique_ptr modules
- **Dependency declarations**: Explicit service dependencies for initialization ordering

### Performance Optimizations:
- **Batch culling operations**: CameraCulling processes entity arrays efficiently avoiding per-entity calls
- **Cached matrix calculations**: Camera component avoids redundant matrix computation via dirty flags
- **Pre-allocated containers**: Camera and viewport storage designed to minimize allocations
- **LOD system integration**: Distance-based level-of-detail for GPU workload optimization

### Multi-Camera System:
- **CameraID-based access**: Unique identifiers enable camera management without pointer instability
- **Active camera concept**: Single primary camera with seamless switching via setActiveCamera()
- **Transition system**: Smooth interpolation between camera states with configurable easing
- **Viewport independence**: Multiple screen regions with independent camera assignments

### Coordinate System Handling:
- **World coordinates**: 3D world space for entity positioning and camera placement
- **Screen coordinates**: 2D pixel coordinates (0,0 top-left) for UI and input processing
- **Viewport coordinates**: Normalized 0.0-1.0 coordinates relative to viewport regions
- **NDC/Clip coordinates**: GPU pipeline compatible coordinate transformations

### Edge Cases & Gotchas:
- **Zero zoom prevention**: Coordinate transformations prevent division by zero with epsilon limits
- **Matrix dirty flag management**: Camera property changes automatically invalidate cached matrices
- **Entity array size matching**: CameraCulling requires transforms.size() == bounds.size() for safety
- **Viewport bounds validation**: Screen point hit testing requires properly configured viewport regions
- **Transition interruption**: New transitions override active ones without state corruption
- **ECS world validity**: All camera operations require valid flecs::world instance

---
**⚠️ Update Requirement**: This file must be updated when:
- Camera service module structure changes (new components, reorganization)
- Camera component data layout changes (position/zoom/matrices/viewSize)
- Service integration APIs change (CameraService public interface modifications)
- Coordinate transformation algorithms change (world↔screen conversion logic)
- Culling or LOD calculation methods change (frustum algorithms, distance calculations)
- ECS component dependencies change (Transform, Bounds, Camera component structure)
- Service locator integration changes (lifecycle, dependency patterns)