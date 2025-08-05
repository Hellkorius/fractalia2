# Fractalia2 Project Documentation

## Overview
Cross-compiled (Linux→Windows) toy engine built to stress-test **tens-of-thousands of discrete entities** at 60 FPS.

## Architecture

## Stack  
- **SDL3** – window + input  
- **Vulkan** – 4× MSAA, dynamic-function loading  
- **Flecs ECS** – entities = `Position`, `Velocity`, `Color`  
- **GLM** – math  
- **MinGW** – build

### ECS Architecture
The project uses Flecs ECS with the following components:
- `Position`: Entity position in 3D space (x, y, z)
- `Velocity`: Entity velocity for movement (x, y, z)
- `Color`: RGBA color values for rendering

Systems:
- `movement_system`: Updates entity positions based on velocity with screen bounds collision

## File Hierarchy

```
fractalia2/
├── CMakeLists.txt              # CMake build configuration for cross-compilation
├── compile-shaders.sh          # Shader compilation script
├── mingw-w64-toolchain.cmake   # Generated CMake toolchain file
├── src/
│   ├── main.cpp                # Main application entry point
	├─ vulkan_renderer.*      // master frame loop
		├──vulkan/
			├─ vulkan_context.*      // instance & device
			├─ vulkan_swapchain.*    // resize, MSAA, depth
			├─ vulkan_pipeline.*     // shaders, renderpass, layout
			├─ vulkan_resources.*    // uniform buffers, descriptor sets
			└─ vulkan_sync.*         // fences/semaphores, cmd pools
│   └── shaders/                # GLSL shader source files
│       ├── vertex.vert         # Vertex shader
│       ├── fragment.frag       # Fragment shader
│       └── compiled/           # Compiled SPIR-V shaders
│           ├── vertex.spv      # Compiled vertex shader
│           └── fragment.spv    # Compiled fragment shader
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

## Hot Spots for Scale
1. `vulkan_resources.cpp` – uniform buffer per frame (currently 64 bytes); bump to UBO array or SSBO for entity data.
2. `recordCommandBuffer()` – single `vkCmdDraw(3,1,0,0)` placeholder; switch to **instanced draw** / **indirect** and bind entity buffer.
3. Flecs system in `main.cpp` – O(n) update loop; profile here first.

### Vulkan Rendering
The project uses Vulkan with dynamic function loading for cross-platform compatibility:
- Vulkan functions are loaded at runtime using SDL3's Vulkan loader
- Shaders are written in GLSL and compiled to SPIR-V using `glslangValidator`
- Run `./compile-shaders.sh` to compile shaders after making changes

### Adding New Components
1. Define component struct in main.cpp
2. Add to entity creation with `.set<ComponentName>({...})`
3. Create corresponding system with `world.system<Components...>()`

### Adding New Systems
Systems follow the pattern:
```cpp
void system_name(flecs::entity e, Component1& comp1, Component2& comp2) {
    // System logic here
}
```

### Shader Development
1. Edit GLSL files in `src/shaders/`
2. Run `./compile-shaders.sh` to compile to SPIR-V
3. Rebuild project to include updated shaders