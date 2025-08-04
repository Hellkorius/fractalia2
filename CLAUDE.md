# Fractalia2 Project Documentation

## Overview
Fractalia2 is a C++ game/graphics project that uses cross-compilation with mingw to build Windows executables on Linux. The project leverages SDL3 for window management and input, Vulkan for rendering, and Flecs ECS for entity-component-system architecture.

## Architecture

### Core Technologies
- **SDL3**: Window management, input handling, Vulkan surface creation
- **Vulkan**: Modern low-level graphics API for high-performance rendering
- **Flecs ECS**: Entity-Component-System architecture for game logic
- **GLM**: Mathematics library for 3D transformations
- **MinGW**: Cross-compilation toolchain for Windows targets

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
├── CLAUDE.md                    # This documentation file
├── CMakeLists.txt              # CMake build configuration for cross-compilation
├── build.sh                    # Full build script with DLL management 
├── build-fast.sh               # Fast incremental build script
├── compile-shaders.sh          # Shader compilation script
├── mingw-w64-toolchain.cmake   # Generated CMake toolchain file
├── src/
│   ├── main.cpp                # Main application entry point
│   ├── vulkan_renderer.h       # Vulkan renderer header
│   ├── vulkan_renderer.cpp     # Vulkan renderer implementation
│   └── shaders/                # GLSL shader source files
│       ├── vertex.vert         # Vertex shader
│       ├── fragment.frag       # Fragment shader
│       └── compiled/           # Compiled SPIR-V shaders
│           ├── vertex.spv      # Compiled vertex shader
│           └── fragment.spv    # Compiled fragment shader
├── include/                    # Header files directory
├── build/                      # Build output directory
│   ├── fractalia2.exe          # Windows executable
│   ├── SDL3.dll                # SDL3 runtime library
│   ├── libgcc_s_seh-1.dll      # MinGW C runtime (if needed)
│   └── libstdc++-6.dll         # MinGW C++ runtime (if needed)
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