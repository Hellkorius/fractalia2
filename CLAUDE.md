# Fractalia2 Codebase Architecture

Cross-compiled (Linux→Windows) engine for 80,000+ entities at 60 FPS using GPU-driven instanced rendering with Structure of Arrays (SoA) optimization and Vulkan frame graphs.

## Project Structure

```
fractalia2/
├── docs/                           (Project documentation with build, architecture, and controls)
├── src/
│   ├── main.cpp, vulkan_renderer.*  (Application entry point and master frame loop coordinator)
│   ├── shaders/                     (GLSL compute and graphics shaders with compiled SPIR-V)
│   ├── ecs/                         (Entity Component System with service-based architecture)
│   │   ├── components/              (Core ECS data structures for GPU synchronization and camera)
│   │   ├── core/                    (Service locator with dependency injection and world management)
│   │   ├── events/                  (Decoupled event bus architecture with type-safe dispatching)
│   │   ├── gpu/                     (CPU-GPU bridge implementing Structure of Arrays for optimal GPU performance)
│   │   ├── services/                (High-level game services coordinating input, camera, and rendering)
│   │   │   ├── camera/              (3D camera system with perspective/orthographic projections, frustum culling, and viewport management)
│   │   │   └── input/               (SDL event processing with action-based input and context switching)
│   │   ├── systems/                 (ECS system infrastructure for entity lifecycle and movement coordination)
│   │   └── utilities/               (Common debugging, profiling, and configuration utilities)
│   └── vulkan/                      (Complete Vulkan subsystem with modern frame graph architecture)
│       ├── core/                    (Foundational Vulkan infrastructure with RAII resource management)
│       ├── monitoring/              (GPU performance monitoring with stress testing and timeout detection)
│       ├── nodes/                   (Frame graph node implementations with BaseComputeNode eliminating 300+ lines of duplication)
│       ├── pipelines/               (Sophisticated pipeline management with caching, hot-reload, and optimization)
│       ├── rendering/               (Frame graph coordination with dependency analysis and barrier optimization)
│       │   ├── compilation/         (Dependency graph analysis with topological sorting and cycle detection)
│       │   ├── execution/           (Memory barrier management for async compute-graphics synchronization)
│       │   └── resources/           (Frame graph resource lifecycle with memory pressure handling)
│       ├── resources/               (Comprehensive resource management infrastructure)
│       │   ├── buffers/             (Buffer lifecycle with staging, transfer orchestration, and statistics)
│       │   ├── core/                (Core resource utilities with validation and coordination patterns)
│       │   ├── descriptors/         (Descriptor set management with update batching and lifecycle tracking)
│       │   └── managers/            (High-level resource coordination for graphics pipeline components)
│       └── services/                (High-level Vulkan services orchestrating frame execution and error recovery)
├── build/                           (CMake build artifacts and compilation outputs)
└── build-fast.sh, compile-shaders.sh (Development automation scripts for cross-compilation and shader processing)
```

## Architecture Overview

**CPU Pipeline**: Flecs ECS → Service Layer → GPU Entity Manager (SoA conversion)  
**GPU Pipeline**: Movement Compute → Physics Compute → Graphics Rendering  
**Coordination**: Frame Graph with automatic barrier insertion and resource dependency tracking  
**Performance**: 80,000+ entities at 60 FPS with cache-optimized 128-byte entity layout and ping-pong buffers

## 3D Camera System

**Camera Component Features**:
- **Dual Projection Support**: Perspective (3D) and orthographic (2D compatibility) modes
- **3D View Control**: Position, target, up vectors with lookAt functionality  
- **Perspective Parameters**: FOV (field of view), aspect ratio, near/far clipping planes
- **3D Movement**: Forward/right/up movement, orbit controls, and smooth transitions
- **Matrix Generation**: Cached view/projection matrices with lazy computation and Vulkan Y-flip correction

**Camera Service Architecture**:
- **CameraManager**: Multi-camera creation, lifecycle, and active camera management
- **CameraTransforms**: 3D coordinate transformations between world, screen, and viewport spaces
- **CameraCulling**: 3D frustum culling for perspective cameras, 2D AABB tests for orthographic
- **ViewportManager**: Multi-viewport support for split-screen rendering with render order control
- **CameraTransitionSystem**: Smooth camera transitions with configurable easing functions

**3D Rendering Pipeline**:
- **View Matrix**: 3D lookAt transformation (`glm::lookAt(position, target, up)`)
- **Projection Matrix**: Perspective projection (`glm::perspective`) with Vulkan coordinate correction
- **Frustum Culling**: 3D visibility testing using camera forward direction and FOV-based angle culling
- **Coordinate Conversion**: Full 3D world-to-screen and screen-to-world transformations

## Key Principles

- **Single Responsibility**: Each component has one well-defined purpose
- **DRY Principle**: BaseComputeNode template method pattern eliminates massive code duplication
- **Structure of Arrays**: GPU-optimized memory layout for 20-30% bandwidth reduction  
- **Service Architecture**: Dependency injection with priority-based initialization
- **Frame Graph**: Declarative rendering with automatic synchronization
- **RAII Resource Management**: Exception-safe Vulkan object lifecycle