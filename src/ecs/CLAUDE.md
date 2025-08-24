# ECS Architecture - 3D Entity Management System

Cross-compiled entity management system supporting 80,000+ entities with GPU-driven 3D movement, perspective camera system, and high-performance rendering pipeline.

## Overview

Service-based ECS architecture coordinating CPU entity lifecycle management with GPU-accelerated 3D movement calculations and perspective rendering. Implements Structure of Arrays (SoA) optimization for 20-30% bandwidth reduction and cache-optimized entity layouts.

**Key Features:**
- **3D Entity Management**: Full 3D positioning, quaternion rotations, and perspective transforms
- **GPU-Driven 3D Movement**: Compute shaders with spherical coordinate random walk patterns
- **3D Camera System**: Perspective projection, lookAt controls, frustum culling, and smooth transitions
- **Service Architecture**: Dependency injection with priority-based initialization
- **Structure of Arrays**: Cache-optimized GPU memory layout with 128-byte entity alignment
- **CPU-GPU Bridge**: Efficient data transfer with ping-pong buffers for async compute-graphics synchronization

## Architecture

```
ECS Pipeline (3D):
CPU (Flecs + Services) → GPU Entity Manager (SoA conversion) → GPU Compute (3D movement) → GPU Graphics (3D rendering)
                     ↓                                      ↓                        ↓
             3D Components        →        3D Movement Centers Buffer  →     Perspective Projection
             3D Transforms        →        3D Velocity Calculations    →     3D Matrix Transforms  
             3D Camera System     →        3D Position Buffers         →     Frustum Culling
```

## Directory Structure

### **components/** - 3D ECS Component Definitions
- **camera_component.h** - 3D camera with perspective projection, lookAt controls, FOV, and near/far planes
- **component.h** - Core 3D components: Transform (position/quaternion/scale), Velocity (3D linear/angular), MovementPattern (3D center points), Bounds (3D AABB)

### **core/** - Service Infrastructure  
- **service_locator.h** - Dependency injection system with service lifecycle management
- **world_manager.cpp/.h** - Flecs world coordination with 3D module initialization and frame execution

### **events/** - Decoupled Event System
- **event_bus.h** - Type-safe event dispatching for 3D camera transitions and input coordination
- **event_types.h** - 3D-specific events (camera movement, position updates, viewport changes)

### **gpu/** - CPU-GPU Bridge with 3D Support
- **gpu_entity_manager.cpp/.h** - Coordinates ECS-to-GPU conversion with 3D movement centers buffer and SoA layout
- **entity_buffer_manager.cpp/.h** - Manages specialized 3D entity buffers (velocity, movement centers, rotation states) 
- **position_buffer_coordinator.cpp/.h** - Ping-pong 3D position buffers for async compute-graphics synchronization
- **specialized_buffers.h** - SoA buffer implementations optimized for 3D entity data (velocity.xyz, movement centers.xyz)

### **services/** - High-Level 3D Game Services

#### **camera/** - 3D Camera System
- **camera_manager.cpp/.h** - 3D camera entity management with perspective/orthographic modes
- **camera_culling.cpp/.h** - 3D frustum culling using perspective projection matrices
- **camera_transforms.cpp/.h** - 3D view/projection matrix calculations with lookAt and perspective transforms
- **camera_transition_system.cpp/.h** - Smooth 3D camera transitions with position/target/up interpolation

#### **input/** - 3D Camera Input System  
- **input_action_system.cpp/.h** - Action mapping supporting 3D camera movement (WASD, mouse look, FOV zoom)
- **input_event_processor.cpp/.h** - SDL event processing with mouse delta tracking for 3D camera rotation

#### **Core Services**
- **camera_service.cpp/.h** - Comprehensive 3D camera service with perspective projection, FOV controls, and multi-camera management
- **input_service.cpp/.h** - Input management with 3D camera action definitions (movement, rotation, FOV adjustment)
- **control_service.cpp/.h** - 3D camera controls integration (WASD movement, mouse look, perspective zoom)
- **rendering_service.cpp/.h** - Render queue management with 3D frustum culling and perspective projection

### **systems/** - ECS System Infrastructure
- **movement_system.cpp/.h** - Movement infrastructure supporting GPU-driven 3D calculations with spherical coordinates
- **lifetime_system.cpp/.h** - Entity lifecycle management with 3D spatial awareness

### **utilities/** - Common ECS Utilities
- **constants.h** - 3D-specific constants (movement amplitudes, camera parameters)
- **debug.h** - 3D debugging utilities for camera positions, entity transforms
- **profiler.h** - Performance monitoring for 3D pipeline stages

## 3D Implementation Details

### **3D Entity Data Flow**
1. **CPU**: Flecs entities with 3D Transform (position/quaternion), 3D Velocity (linear/angular), 3D MovementPattern (center point)
2. **GPU Bridge**: GPUEntityManager converts to Structure of Arrays with 3D movement centers buffer
3. **GPU Compute**: Movement shader generates 3D velocity using spherical coordinates (elevation angle, azimuth angle, radius)
4. **GPU Graphics**: Physics shader integrates 3D positions, graphics pipeline applies 3D perspective transforms

### **3D Camera Architecture**
- **Position/Target/Up**: Full 3D camera positioning with lookAt functionality
- **Perspective Projection**: FOV-based projection with configurable near/far planes  
- **Frustum Culling**: 3D visibility testing using perspective projection matrices
- **Multi-Camera**: Support for multiple 3D viewports with independent cameras
- **Smooth Transitions**: Interpolation between 3D camera states (position, target, orientation)

### **GPU 3D Movement System**  
- **Spherical Coordinates**: Random walk using elevation and azimuth angles for natural 3D movement
- **3D Velocity Generation**: `velocity.xyz = speed * [cosElev*cos(θ), cosElev*sin(θ), sin(elev)]`
- **3D Movement Centers**: Each entity has a 3D center point for bounded random walk behavior
- **3D Position Integration**: Physics shader: `position.xyz += velocity.xyz * deltaTime`

### **Structure of Arrays (3D Optimized)**
```cpp
struct GPUEntitySoA {
    std::vector<glm::vec4> velocities;        // velocity.xyz, damping
    std::vector<glm::vec4> movementCenters;   // center.xyz, reserved (3D movement origins)
    std::vector<glm::vec4> rotationStates;    // rotation, angularVelocity.xyz
    // ... other SoA buffers
};
```

### **Service Dependencies (3D Pipeline)**
```
WorldManager (Priority: 100) → Core ECS with 3D components
    ↓
InputService (Priority: 90) → 3D camera input actions  
    ↓
CameraService (Priority: 80) → 3D perspective cameras, frustum culling
    ↓  
RenderingService (Priority: 70) → 3D render queue, perspective projection
    ↓
ControlService (Priority: 60) → 3D camera movement coordination
```

## Performance Optimizations

- **Cache-Optimized SoA**: 3D velocity/position data packed for GPU vectorization
- **Spherical Coordinates**: Efficient 3D random movement generation in compute shaders  
- **Ping-Pong Buffers**: Async 3D position updates between compute and graphics
- **3D Frustum Culling**: Early rejection of entities outside camera perspective
- **Batch 3D Transforms**: Matrix operations vectorized for multiple entities

## Integration Notes

- **Frame Graph**: Automatic barrier insertion for 3D position buffer transitions
- **Resource Management**: RAII cleanup for 3D camera and buffer resources
- **3D Debug Support**: Camera position readback, entity transform visualization
- **Cross-Platform**: MinGW compatibility with 3D mathematics and OpenGL interop