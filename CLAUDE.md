# Fractalia2 Project Documentation

## Overview
Cross-compiled (Linux→Windows) toy engine built to stress-test **hundreds-of-thousands of discrete 2D entities** with GPU compute-driven physics at 60 FPS.

## Architecture

## Stack  
- **SDL3** – window + input  
- **Vulkan** – 4× MSAA, dynamic-function loading, **compute shaders**
- **Flecs ECS** – CPU entity management + GPU entity handover
- **GLM** – math  
- **MinGW** – build

### Hybrid CPU/GPU Architecture
The project uses a hybrid approach:
- **CPU (Flecs ECS)**: Entity creation, input handling, camera controls, system management
- **GPU (Compute)**: All movement calculations, dynamic color generation, transform updates
- **GPU (Graphics)**: Direct rendering from compute-updated entity buffers

#### Entity Components (CPU-side):
- `Transform`: Entity transform matrix and position
- `Renderable`: Shape type, color, layer, visibility
- `MovementPattern`: Movement parameters (amplitude, frequency, phase, etc.)
- `Velocity`: Linear/angular velocity (legacy CPU movement)

#### GPU Entity Structure:
- `GPUEntity`: 128-byte structure containing transform matrix, color, movement parameters, and runtime state

## File Hierarchy

```
fractalia2/
├── CMakeLists.txt              # CMake build configuration for cross-compilation
├── compile-shaders.sh          # Shader compilation script
├── mingw-w64-toolchain.cmake   # Generated CMake toolchain file
├── src/
│   ├── main.cpp                # Main application entry point
	├─ PolygonFactory.*			# Polygon generation
	├─ vulkan_renderer.*      // master frame loop + GPU compute dispatch
		├──vulkan/
			├─ vulkan_context.*      // instance & device
			├─ vulkan_swapchain.*    // resize, MSAA, depth
			├─ vulkan_pipeline.*     // graphics shaders, renderpass, layout
			├─ vulkan_resources.*    // uniform buffers, descriptor sets
			├─ vulkan_sync.*         // fences/semaphores, cmd pools
			└─ compute_pipeline.*    // compute shader pipeline management
	ecs/
	├── component.hpp           // All component definitions
	├── entity.hpp              // Entity & EntityHandle aliases
	├──	entity_factory.hpp		// Factory for creating and managing entities
	├── gpu_entity_manager.*    // GPU entity storage and CPU->GPU handover
	├── memory_manager.hpp		// memory manager for ECS
	├── profiler.hpp			// performance data collection
	├── render_batch.hpp		// RenderInstance and RenderBatch (legacy CPU path)
	├── world.hpp               // World wrapper around flecs::world
	├── system.hpp              // Base System interface
	├── system_scheduler.hpp
	├──	camera_component.hpp
	└── systems/
		├── physics_system.*  	// Movement & rotation logic (legacy)
		├── input_system.*		// input handling
		├── movement_system.* 	// CPU movement (disabled - now GPU compute)
		├── camera_system.*		// camera controls
		├──	control_handler_system.* // controls + GPU entity creation
		└── render_system.cpp   // Vulkan-aware RenderSystem (legacy CPU path)
│   └── shaders/                # GLSL shader source files
│       ├── vertex.vert         # Vertex shader (updated for GPUEntity)
│       ├── fragment.frag       # Fragment shader
│       ├── movement.comp       # **NEW: GPU compute shader for entity movement**
│       └── compiled/           # Compiled SPIR-V shaders
│           ├── vertex.spv      # Compiled vertex shader
│           ├── fragment.spv    # Compiled fragment shader
│           └── movement.spv    # **NEW: Compiled compute shader**
├── include/                    # Header files directory
└── ../vendored/                # External libraries
    ├── SDL3-3.1.6/             # SDL3 source and binaries
    │   └── x86_64-w64-mingw32/  # Windows 64-bit binaries
    ├── glm/                     # GLM mathematics library
    ├── flecs-4.0.2/             # Flecs ECS library
    ├── vulkan/                  # Vulkan headers
    │   └── include/             # Vulkan header files
    └── Vulkan-Loader/           # Vulkan loader source (for reference)
```


## Development Notes

## GPU Compute Architecture

### Entity Data Flow
1. **Entity Creation**: CPU creates entities via `EntityFactory` with `Transform`, `Renderable`, `MovementPattern`
2. **GPU Handover**: `GPUEntityManager.addEntitiesFromECS()` converts to `GPUEntity` format and uploads to GPU storage buffers
3. **Compute Dispatch**: Each frame, `movement.comp` shader processes all entities in parallel (64 threads/workgroup)
4. **Graphics Rendering**: Vertex shader reads directly from compute-updated `GPUEntity` buffers

### GPUEntity Structure (128 bytes)
```cpp
struct GPUEntity {
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
    glm::vec4 color;              // 16 bytes - RGBA color  
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, reserved
};
```

### Movement Patterns (Extensible)
- **MOVEMENT_PETAL** (0): Smooth radial oscillation from center
- **MOVEMENT_ORBIT** (1): Circular orbit around center point
- **MOVEMENT_WAVE** (2): Sinusoidal wave motion
- Easy to add new patterns by extending the compute shader switch statement

### Critical Implementation Details
- **Double-buffered Storage**: Compute reads from buffer N, writes to buffer N+1, graphics renders buffer N+1
- **Memory Barriers**: `VK_ACCESS_SHADER_WRITE_BIT` → `VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT` synchronization between compute/graphics
- **Buffer Layout**: Vertex shader attributes match `GPUEntity` exactly (locations 2-9 for instance data)
- **Dynamic Function Loading**: All Vulkan functions loaded at runtime for cross-platform compatibility
- **Shader Path**: Compute shader loaded from `shaders/compiled/movement.spv` (not `src/shaders/compiled/`)

### Performance Characteristics
- **Current Scale**: 10,000 entities at 60 FPS with smooth movement and dynamic colors
- **Theoretical Limit**: 131k entities (16MB buffer / 128 bytes per entity)
- **GPU Utilization**: Parallel compute across thousands of threads vs sequential CPU processing
- **Memory Bandwidth**: Direct GPU-to-GPU data flow eliminates CPU→GPU transfer bottleneck

### Controls (Updated for GPU System)
- **ESC**: Exit
- **+/=**: Add 1000 more GPU entities (stress test up to 131k limit)
- **-**: Show current GPU performance stats (CPU entities vs GPU entities)
- **Left Click**: Create GPU entity with movement at mouse position
- **P**: Print detailed performance report
- **WASD**: Move camera
- **Mouse Wheel**: Zoom in/out
- **Space**: Reset camera to origin

### Vulkan Rendering
The project uses Vulkan with dynamic function loading for cross-platform compatibility:
- Vulkan functions are loaded at runtime using SDL3's Vulkan loader
- Graphics + Compute pipelines with synchronized command buffer execution
- Shaders are written in GLSL and compiled to SPIR-V using `glslangValidator`
- Run `./compile-shaders.sh` to compile all shaders after making changes

### Adding New GPU Movement Patterns
1. Add new `MOVEMENT_TYPE` constant in `movement.comp`
2. Implement movement function in compute shader (e.g., `computeSpiral()`)
3. Add to switch statement in `main()` function
4. Update entity creation to use new movement type
5. Recompile shaders with `./compile-shaders.sh`

### GPU Entity Development
1. **CPU Entity Creation**: Use `EntityFactory.createSwarm()` or `createMovingEntity()`
2. **GPU Upload**: Call `renderer.getGPUEntityManager()->addEntitiesFromECS(entities)`
3. **Runtime Creation**: Control handler automatically uploads new entities each frame
4. **Debugging**: Use `-` key to monitor GPU entity count vs CPU entity count

### Shader Development
1. Edit GLSL files in `src/shaders/` (vertex.vert, fragment.frag, movement.comp)
2. Run `./compile-shaders.sh` to compile to SPIR-V
3. Rebuild project to include updated shaders
4. **Note**: Compute shader path is `shaders/compiled/movement.spv` (not `src/shaders/compiled/`)