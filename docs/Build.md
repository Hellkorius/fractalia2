## Build System

### Build Scripts

#### build.sh (Full Build)
- Generates CMake toolchain file for mingw
- Configures and builds the project

#### build-fast.sh (Fast Incremental Build)
**Speed optimizations:**
- Compiles Flecs ECS as a separate static library (built once, reused)
- Skips DLL copying (assumes DLLs are already in place)
- Uses Ninja build system if available (faster than Make)
- Enables ccache if available for compiler caching
- Skips CMake configuration if unchanged
- Uses optimal parallel job count (CPU cores + 2)
- Only recompiles changed source files

## Usage

### Building

#### Full Build (first time or after major changes)
```bash
./build.sh
```

#### Fast Incremental Build (for development)
```bash
./build-fast.sh
```

**When to use each:**
- Use `build.sh` for first build, after library changes, or when you need DLLs copied
- Use `build-fast.sh` for regular development (2-10x faster for incremental builds)

### Running
Execute `build/fractalia2.exe` on Windows. The executable should run with a moving red triangle that bounces off screen edges.

Shader Compilation and Loading

  The project uses GLSL shaders compiled to SPIR-V format for Vulkan rendering.

  Shader Compilation:
  - Source shaders are located in src/shaders/ (vertex.vert, fragment.frag)
  - Run ./compile-shaders.sh to compile shaders using glslangValidator
  - Compiled SPIR-V shaders are output to build/shaders/compiled/

  Shader Loading:
  - The Vulkan renderer loads compiled shaders from shaders/compiled/ relative to the executable
  - Shaders must be recompiled after any GLSL source changes
  - The app expects vertex.spv and fragment.spv in the shaders directory alongside the executable

  Build Process:
  1. Compile shaders: ./compile-shaders.sh
  2. Build executable: standard CMake build process
  3. Both executable and shaders reside in the build/ directory for distribution