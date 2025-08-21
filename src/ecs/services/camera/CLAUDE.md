# Camera Service Module

## Purpose
Multi-camera system with smooth transitions, frustum culling, and viewport support for GPU-driven rendering pipeline. Provides unified camera management through service-based architecture.

## Core Components

### CameraService (`camera_service.h`)
**Main interface** aggregating all camera functionality via unique_ptr composition:
- `CameraManager` - ECS entity lifecycle and active camera tracking
- `CameraTransitionSystem` - Smooth interpolated camera state changes
- `ViewportManager` - Multi-viewport split-screen support
- `CameraCulling` - Frustum culling and LOD calculations
- `CameraTransforms` - Coordinate transformations and matrix generation

### Key Classes

#### CameraManager
**Purpose**: Camera entity lifecycle in ECS
- **Creates**: `CameraID` (uint32_t) for camera identification
- **Manages**: `flecs::entity` with Camera components, active camera state
- **Methods**: `createCamera()`, `setActiveCamera()`, movement/zoom/rotation controls
- **Pattern**: ID-based access preventing pointer instability

#### CameraCulling  
**Purpose**: Visibility optimization for rendering pipeline
- **Input**: `std::vector<Transform>`, `std::vector<Bounds>`, `Camera*`
- **Output**: `std::vector<CullingInfo>` with visibility/distance/LOD data
- **Method**: `performFrustumCulling()` - batch processes entity arrays
- **LOD**: Distance-based levels (0-3) with configurable thresholds

#### CameraTransitionSystem
**Purpose**: Smooth camera state interpolation
- **Types**: `INSTANT`, `LINEAR`, `SMOOTH_STEP`, `EASE_IN/OUT`, `SPRING`
- **State**: `CameraTransition` struct with duration/callbacks/custom easing
- **Method**: `transitionToCamera()` - interpolates position/zoom/rotation
- **Pattern**: Frame-based updates with completion callbacks

#### CameraTransforms
**Purpose**: Coordinate system conversions
- **Conversions**: `worldToScreen()`, `screenToWorld()`, `viewportToWorld()`
- **Matrices**: `getViewMatrix()`, `getProjectionMatrix()`, `getViewProjectionMatrix()`
- **Integration**: Provides matrices to GPU pipeline via RenderingService

#### ViewportManager
**Purpose**: Multi-viewport split-screen rendering
- **Data**: `Viewport` struct with normalized coordinates (0.0-1.0)
- **Features**: Render order, clear settings, camera assignments
- **Methods**: `createViewport()`, `findViewportAtScreenPoint()` for input routing

## Data Flow

### Frame Update Cycle
```
CameraService::update(deltaTime)
  → CameraTransitionSystem::update() [interpolates active transitions]
  → updateActiveCamera() [applies transition results]
  → CameraTransforms provides matrices to RenderingService
  → CameraCulling processes entity batches for visibility
  → ViewportManager provides screen regions for multi-pass rendering
```

### Service Integration
- **InputService** → coordinate transformations for mouse/touch input
- **RenderingService** → consumes view-projection matrices and culling results
- **ControlService** → triggers camera movements and transitions
- **WorldManager** → provides ECS world for camera entity management

## Key Data Structures

### Camera Component (in `camera_component.h`)
```cpp
struct Camera {
    glm::vec3 position;           // World position
    float zoom;                   // Zoom level (no limits)
    float rotation;               // Rotation in radians
    glm::vec2 viewSize;          // Orthographic view dimensions
    
    // Lazy-computed matrices (dirty flags)
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    bool matricesDirty = true;
};
```

### CullingInfo
```cpp
struct CullingInfo {
    glm::vec3 position;
    glm::vec3 bounds;
    bool visible;
    float distanceToCamera;
    int lodLevel;                 // 0=highest detail, 3=lowest
};
```

### Viewport
```cpp
struct Viewport {
    std::string name;
    CameraID cameraID;
    glm::vec2 offset;            // Normalized (0.0-1.0)
    glm::vec2 size;              // Normalized (0.0-1.0)
    int renderOrder;
    glm::vec4 clearColor;
};
```

## Essential Patterns

### Service Registration
```cpp
// In service initialization
auto cameraService = ServiceLocator::instance()
    .createAndRegister<CameraService>("CameraService", 80);
serviceLocator.declareDependencies<CameraService, WorldManager>();

// Usage
SERVICE(CameraService).createCamera("main");
SERVICE(CameraService).transitionToCamera(cameraID, LINEAR, 2.0f);
```

### Camera Operations
```cpp
// Create and manage cameras
CameraID mainCam = cameraService.createCamera("main");
cameraService.setActiveCamera(mainCam);
cameraService.setCameraPosition(mainCam, {0, 0, 10});

// Smooth transitions
cameraService.transitionToCamera(targetCam, EASE_IN_OUT, 1.5f);

// Coordinate transformations
glm::vec2 screenPos = cameraService.worldToScreen(worldPos, screenSize);
glm::mat4 mvp = cameraService.getViewProjectionMatrix();
```

### Culling Integration
```cpp
// In rendering pipeline
std::vector<CullingInfo> visible = cameraService.performFrustumCulling(
    entityTransforms, entityBounds);
    
// Filter by visibility and LOD
for (const auto& info : visible) {
    if (info.visible && info.lodLevel <= maxLOD) {
        renderEntity(info);
    }
}
```

## Performance Optimizations

1. **Batch Culling**: Process entity arrays efficiently vs per-entity calls
2. **Lazy Matrix Computation**: Camera matrices calculated only when dirty
3. **ID-Based Access**: Stable camera references without pointer invalidation
4. **LOD System**: Distance-based detail reduction for GPU workload optimization
5. **Service Composition**: Modular design enables efficient sub-system access

## Dependencies

- **Core**: `flecs.h` (ECS), `glm/` (math), `service_locator.h` (DI pattern)
- **Components**: `camera_component.h`, `component.h` (Transform/Bounds)
- **Pipeline**: Integrates with VulkanRenderer and RenderingService for GPU rendering