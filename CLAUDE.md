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
│   │   │   ├── camera/              (Multi-camera management with frustum culling and smooth transitions)
│   │   │   └── input/               (SDL event processing with action-based input and context switching)
│   │   ├── systems/                 (ECS system infrastructure for entity lifecycle and movement coordination)
│   │   └── utilities/               (Common debugging, profiling, and configuration utilities)
│   └── vulkan/                      (Complete Vulkan subsystem with modern frame graph architecture)
│       ├── core/                    (Foundational Vulkan infrastructure with RAII resource management)
│       ├── monitoring/              (GPU performance monitoring with stress testing and timeout detection)
│       ├── nodes/                   (Frame graph node implementations for compute, graphics, and presentation)
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

## Key Principles

- **Single Responsibility**: Each component has one well-defined purpose
- **Structure of Arrays**: GPU-optimized memory layout for 20-30% bandwidth reduction  
- **Service Architecture**: Dependency injection with priority-based initialization
- **Frame Graph**: Declarative rendering with automatic synchronization
- **RAII Resource Management**: Exception-safe Vulkan object lifecycle