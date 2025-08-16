# Fractalia2 Project Documentation

## Overview
Cross-compiled (Linux→Windows) toy engine built to stress-test **hundreds-of-thousands of discrete 2D entities** with GPU-driven instanced rendering at 60 FPS.

## Stack  
- **SDL3** – window + input  
- **Vulkan** – 4× MSAA, instanced rendering, modular design
- **Flecs ECS** – CPU entity management + GPU handover
- **GLM** – math  
- **MinGW** – cross-compilation

## Architecture

### Hybrid CPU/GPU Design
- **CPU (Flecs ECS)**: Entity lifecycle, input handling, camera controls
- **GPU Compute**: Movement calculation for all entity types using compute shaders
- **GPU Graphics**: Instanced rendering with pre-computed positions, unified buffer management
- **Bridge**: GPUEntityManager handles CPU→GPU data transfer and compute→graphics synchronization

### Core Systems
- **Direct Flecs**: Native `world.system<>().each()` patterns, no abstractions
- **Input System**: SDL events → world coordinates, key bindings
- **Camera System**: View/projection matrix management, screen transforms  
- **Control System**: Singleton state for runtime entity creation/deletion
- **Movement Commands**: Thread-safe command queue for GPU entity operations

### Entity Data Flow
1. **CPU Entities**: Flecs components (Transform, Renderable, MovementPattern)
2. **GPU Upload**: GPUEntityManager converts ECS → GPUEntity structs  
3. **Compute Pass**: Modular movement compute shaders calculate positions based on movement type
4. **GPU Storage**: Entity buffer (128-byte GPUEntity layout) + Position buffer (16-byte vec4 per entity)
5. **Graphics Pass**: Vertex shader reads pre-computed positions
6. **Rendering**: Single draw call with instanced vertex data

## File Structure

```
fractalia2/
├── CMakeLists.txt              # Cross-compilation build config
├── build-fast.sh               # Quick development builds
├── compile-shaders.sh          # Shader compilation 
├── Controls.md                 # Input controls documentation
├── src/
│   ├── main.cpp                # Application entry, SDL+Vulkan setup
│   ├── vulkan_renderer.*       # Master renderer, frame loop
│   ├── PolygonFactory.*        # Geometric shape generation
│   ├── graphicstests.*         # Graphics validation tests
│   ├── vulkan/                 # Modular Vulkan subsystems
│   │   ├── vulkan_context.*    # Instance, device, queue setup
│   │   ├── vulkan_swapchain.*  # Swapchain, MSAA, framebuffers
│   │   ├── vulkan_pipeline.*   # Graphics pipeline, shaders
│   │   ├── vulkan_resources.*  # Buffers, descriptors, uniforms
│   │   ├── vulkan_sync.*       # Fences, semaphores, command pools
│   │   ├── resource_context.*  # Unified buffer/memory manager
│   │   ├── command_executor.*  # Staging buffer operations
│   │   ├── vulkan_function_loader.* # Dynamic Vulkan loading
│   │   ├── vulkan_utils.*      # File I/O, validation
│   │   └── vulkan_constants.h  # Constants, limits
│   ├── ecs/                    # ECS components and systems
│   │   ├── component.h         # All ECS component definitions
│   │   ├── entity.*            # Entity handles and aliases
│   │   ├── entity_factory.h    # Batch entity creation
│   │   ├── gpu_entity_manager.* # CPU→GPU entity bridge
│   │   ├── movement_command_system.* # Thread-safe GPU commands
│   │   ├── camera_component.h  # Camera matrices
│   │   ├── constants.h         # ECS limits, movement types
│   │   ├── debug.h             # Unified DEBUG_LOG macro
│   │   ├── memory_manager.h    # Memory usage stats
│   │   ├── profiler.h          # Performance monitoring
│   │   ├── system_scheduler.h  # System registration helper
│   │   └── systems/            # Individual ECS systems
│   │       ├── input_system.*  # SDL input → world coords
│   │       ├── camera_system.* # Camera movement, matrices
│   │       ├── lifetime_system.* # Entity lifecycle
│   │       ├── simple_control_system.* # Runtime controls
│   │       └── systems_common.h # Shared system headers
│   └── shaders/
│       ├── vertex.vert         # Vertex shader (reads pre-computed positions)
│       ├── fragment.frag       # Fragment shading
│       ├── movement_pattern.comp # Compute shader for pattern movements (types 0-3)
│       ├── movement_random.comp  # Compute shader for random walk (type 4)
│       └── compiled/           # SPIR-V bytecode
```


## Core Concepts

### GPUEntity Structure (128-byte layout)
```cpp
struct GPUEntity {
    glm::mat4 modelMatrix;        // 64 bytes - world transform
    glm::vec4 color;              // 16 bytes - RGBA color  
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, stateTimer, entityState
};
```

### Development Workflow
1. **Entity Creation**: `EntityFactory::createSwarm()` → Flecs components
2. **GPU Upload**: `GPUEntityManager::addEntitiesFromECS()` → GPU entity buffer
3. **Runtime Commands**: Thread-safe `MovementCommandProcessor` for GPU operations
4. **Compute Dispatch**: Modular compute shader selection based on movement type, single pipeline dispatch
5. **Graphics Pipeline**: Vertex shader reads pre-computed positions for rendering
6. **Rendering**: Single instanced draw call processes all entities
7. **Debug**: `-` key shows CPU vs GPU entity counts

### Key Systems
- **Input**: SDL events → world coordinates, camera controls
- **Camera**: View/projection matrices, zoom, pan
- **Control**: Runtime entity creation/deletion via key bindings (Keys 0-4 for movement types)
- **GPU Bridge**: Efficient CPU→GPU data transfer with validation
- **Movement**: Thread-safe command queue for dynamic entity operations
- **Modular Compute Pipeline**: Dynamic shader selection with pattern movement (types 0-3) and random walk (type 4) pipelines