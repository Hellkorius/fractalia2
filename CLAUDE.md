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
- **GPU (Graphics)**: Direct rendering, All movement calculations, dynamic color generation, transform updates

### System Architecture
- **Direct Flecs Systems**: Native `world.system<>().each()` pattern, no wrappers
- **SimpleControlSystem**: Singleton state pattern for control flags
- **GPU Operations**: Direct calls in main loop after Flecs execution
- **ECS**: Pure Flecs idioms

#### Control Flow
- **Flecs Systems**: Direct `.each()` execution via `world.progress(deltaTime)`
- **Main Loop**: Process control flags, execute GPU operations
- **No Scheduling Complexity**: Standard Flecs system ordering

#### Entity Components (CPU-side):
- `Transform`: Entity transform matrix and position
- `Renderable`: Shape type, color, layer, visibility
- `MovementPattern`: Movement parameters (amplitude, frequency, phase, etc.)

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
			├─ resource_context.*    // centralized resource management (buffers, images, staging)
			├─ command_executor.*    // lightweight command buffer executor for staging copies
			├─ vulkan_constants.h    
			└─ compute_pipeline.*    // compute shader pipeline management
	ecs/
	├── component.h           // All component definitions
	├── entity.h              // Entity & EntityHandle aliases
	├──	entity_factory.h		// Factory for creating and managing entities
	├── gpu_entity_manager.*    // GPU entity storage and CPU->GPU handover
	├── movement_command_system.* // Thread-safe movement command queue and processor
	├── memory_manager.h		// minimal statistics helper (69 lines)
	├── profiler.h			// performance data collection
	├── system_scheduler.h    // phase manager for direct system registration (110 lines)
	├──	camera_component.h
	├── debug.h				// unified DEBUG_LOG macro for project
	├── constants.h			// system-wide constants (batch sizes, movement types)
	└── systems/
		├── lifetime_system.*  	// Entity lifetime management
		├── input_system.*		// input handling + screen-to-world conversion
		├── camera_system.*		// camera controls
		├── simple_control_system.* // input handling + GPU entity operations
		└── systems_common.h	// shared headers for system files
│   └── shaders/                # GLSL shader source files
│       ├── vertex.vert         # Vertex shader (processes movement)
│       └── fragment.frag       # Fragment shader
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