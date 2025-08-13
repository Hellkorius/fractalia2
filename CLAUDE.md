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
- **CPU (Flecs ECS)**: Entity creation, input handling, camera controls, system scheduling
- **GPU (Compute)**: All movement calculations, dynamic color generation, transform updates
- **GPU (Graphics)**: Direct rendering from compute-updated entity buffers

### System Architecture
- **Direct Flecs Systems**: Native `world.system<>().each()` pattern, no wrappers
- **SimpleControlSystem**: Singleton state pattern for control flags
- **GPU Operations**: Direct calls in main loop after Flecs execution
- **Minimal ECS**: Pure Flecs idioms, ~200 lines total ECS infrastructure

#### Control Flow
- **Flecs Systems**: Direct `.each()` execution via `world.progress(deltaTime)`
- **Main Loop**: Process control flags, execute GPU operations
- **No Scheduling Complexity**: Standard Flecs system ordering

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
├── Controls.md 				
├── compile-shaders.sh          # Shader compilation script
├── mingw-w64-toolchain.cmake   # Generated CMake toolchain file
├── src/
│   ├── main.cpp                # Main application entry point
	├─ PolygonFactory.*			# Polygon generation
	├─ vulkan_renderer.*      // master frame loop + GPU compute dispatch
		├──vulkan/
			├─ vulkan_function_loader.* // centralized Vulkan function loading
			├─ vulkan_utils.*        // utility functions (memory, file I/O)
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
	├── movement_command_system.* // Thread-safe movement command queue and processor
	├── memory_manager.hpp		// minimal statistics helper (69 lines)
	├── profiler.hpp			// performance data collection
	├── system_scheduler.hpp    // phase manager for direct system registration (110 lines)
	├──	camera_component.hpp
	├── debug.hpp				// unified DEBUG_LOG macro for project
	├── constants.hpp			// system-wide constants (batch sizes, movement types)
	└── systems/
		├── lifetime_system.*  	// Entity lifetime management
		├── input_system.*		// input handling + screen-to-world conversion
		├── camera_system.*		// camera controls
		├── simple_control_system.* // input handling + GPU entity operations
		└── systems_common.hpp	// shared headers for system files
│   └── shaders/                # GLSL shader source files
│       ├── vertex.vert         # Vertex shader (keyframe-based rendering)
│       ├── fragment.frag       # Fragment shader
│       ├── movement.comp       # GPU compute shader for entity movement (legacy)
│       ├── movement_keyframe.comp # GPU compute shader for keyframe generation
│       └── compiled/           # Compiled SPIR-V shaders
│           ├── vertex.spv      # Compiled vertex shader
│           ├── fragment.spv    # Compiled fragment shader
│           ├── movement.spv    # Compiled compute shader (legacy)
│           └── movement_keyframe.spv # Compiled keyframe shader
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

### Keyframe-Based Rendering System
1. **Entity Creation**: CPU creates entities via `EntityFactory` with `Transform`, `Renderable`, `MovementPattern`
2. **GPU Handover**: `GPUEntityManager.addEntitiesFromECS()` converts to `GPUEntity` format and uploads to entity buffer
3. **Keyframe Generation**: `movement_keyframe.comp` pre-computes 100 frames of position, rotation, and color data
4. **Graphics Rendering**: Vertex shader reads from keyframe buffer and interpolates between frames
5. **Staggered Updates**: Only 1% of keyframes updated per frame for performance

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
- **Keyframe Buffer**: 100 frames × N entities × 32 bytes (position + rotation + color)
- **Staggered Computation**: Only updates keyframes for entities where `(entityID + frame) % 100 == 0`
- **Memory Barriers**: Synchronization between keyframe compute and vertex shader reads
- **Vertex Shader**: Reads keyframe data directly, builds transform matrix on-the-fly
- **Centralized Function Loading**: `VulkanFunctionLoader` provides single source of truth for all Vulkan function pointers
- **Shader Paths**: `movement_keyframe.spv` for keyframe generation, `vertex.vert` for keyframe rendering

### Performance Characteristics
- **Current Scale**: 90,000 entities at 60 FPS with smooth movement and dynamic colors
- **Theoretical Limit**: 131k entities (16MB buffer / 128 bytes per entity)
- **GPU Utilization**: Parallel compute across thousands of threads vs sequential CPU processing
- **Memory Bandwidth**: Direct GPU-to-GPU data flow eliminates CPU→GPU transfer bottleneck


### Vulkan Rendering
The project uses Vulkan with dynamic function loading for cross-platform compatibility:
- **SDL-Vulkan Integration**: Pure Vulkan presentation, no SDL swap functions
- **Low-Latency Optimizations**: MAILBOX/IMMEDIATE present modes, optimized fence waits
- **Frame Synchronization**: Two-tier timeout strategy (immediate check → 16ms wait)
- **Swapchain Buffering**: Optimized image count for smooth 60fps presentation
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
3. **Movement Commands**: Use thread-safe command queue via `renderer.getMovementCommandProcessor()`
4. **Runtime Creation**: Control handler automatically uploads new entities each frame
5. **Debugging**: Use `-` key to monitor GPU entity count vs CPU entity count

### Movement Command System
- **Thread-Safe Queue**: `MovementCommandQueue` with mutex-protected operations
- **Command Processor**: `MovementCommandProcessor` with comprehensive error handling
- **Synchronization**: Commands processed at GPU fence sync point (same timing as original workaround)
- **Performance**: Pre-allocated buffers, atomic flags, batch processing limits
- **Error Handling**: Validation, exception handling, monitoring statistics

### Shader Development
1. Edit GLSL files in `src/shaders/` (vertex.vert, fragment.frag, movement_keyframe.comp)
2. Run `./compile-shaders.sh` to compile to SPIR-V
3. Rebuild project to include updated shaders
4. **Note**: Keyframe shader path is `shaders/compiled/movement_keyframe.spv`