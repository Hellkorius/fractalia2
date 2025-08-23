# Camera Services Directory

(Modular camera system providing multi-camera management, coordinate transformations, frustum culling, smooth transitions, and viewport support for split-screen rendering)

## Folder Hierarchy

```
src/ecs/services/camera/
├── camera.md                       # Documentation file
├── camera_culling.h/cpp           # Frustum culling system
├── camera_manager.h/cpp           # Camera entity lifecycle management
├── camera_transforms.h/cpp        # World/screen coordinate transformations
├── camera_transition_system.h/cpp # Smooth camera state transitions
└── viewport_manager.h/cpp         # Multi-viewport screen region management
```

## Files

### camera.md
**Inputs:** None (documentation file)
**Outputs:** Design documentation for the camera services architecture, component integration patterns, and API references.

### camera_culling.h
**Inputs:** Transform and Bounds component references, Camera state for frustum calculations.
**Outputs:** CullingInfo struct, CameraBounds struct, visibility test interfaces. Provides frustum culling and camera bounds calculation declarations.

### camera_culling.cpp
**Inputs:** Entity transforms, bounding boxes, camera position/zoom/viewSize parameters.
**Outputs:** Boolean visibility results, calculated camera frustum bounds with validity flags. Performs 2D frustum culling using AABB intersection tests.

### camera_manager.h
**Inputs:** Flecs ECS world reference, Camera component data, string identifiers.
**Outputs:** CameraID handles, camera entity management interface, active camera tracking. Declares multi-camera lifecycle management with name-based lookup.

### camera_manager.cpp
**Inputs:** ECS world for entity creation, Camera structs, window resize events, movement/zoom requests.
**Outputs:** Camera entities in ECS world, CameraID mappings, active camera state updates. Manages camera entity lifecycle with aspect ratio updates and cleanup ordering.

### camera_transforms.h
**Inputs:** World positions, screen coordinates, Camera and Viewport state references.
**Outputs:** Coordinate transformation interface, matrix accessor declarations. Provides world↔screen coordinate conversion and matrix calculation methods.

### camera_transforms.cpp
**Inputs:** 3D world positions, 2D screen coordinates, camera position/zoom/viewSize, viewport configurations.
**Outputs:** Converted coordinate vectors, view/projection matrices from camera state. Handles coordinate transformations with proper Y-axis flipping for SDL screen space.

### camera_transition_system.h
**Inputs:** Delta time, source/target camera states, transition configuration parameters.
**Outputs:** CameraTransition struct, easing function interface, interpolated camera states. Declares smooth camera transitions with multiple easing curve types.

### camera_transition_system.cpp
**Inputs:** Frame delta time, Camera start/end states, transition duration and type parameters.
**Outputs:** Interpolated camera positions/zoom/rotation, transition completion callbacks. Implements easing functions and camera state interpolation with angle wrapping.

### viewport_manager.h
**Inputs:** Viewport configuration, camera assignments, screen coordinates for hit testing.
**Outputs:** Viewport struct with screen rectangle calculations, multi-viewport management interface. Declares viewport creation and screen region management for split-screen support.

### viewport_manager.cpp
**Inputs:** Named viewport configurations, screen size, point coordinates for viewport hit testing.
**Outputs:** Active viewport collections sorted by render order, screen point collision results. Manages viewport lifecycle with normalized coordinate conversion to pixel space.