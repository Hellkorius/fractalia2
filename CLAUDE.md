# Fractalia2 Project Documentation

## Overview
Cross-compiled (Linux→Windows) engine for 80,000+ entities at 60 FPS using GPU-driven instanced rendering.

## Stack
- SDL3, Vulkan, Flecs ECS, GLM, MinGW

## Architecture

### CPU/GPU Pipeline
1. **CPU (Flecs ECS)**: Entity lifecycle, input, camera
2. **GPU Compute**: Random walk movement calculation  
3. **GPU Graphics**: Instanced rendering
4. **Bridge**: GPUEntityManager handles CPU→GPU transfer

### Data Flow
1. Flecs components → GPUEntity structs (128-byte layout)
2. Compute shader calculates positions with interpolation  
3. Graphics reads entity buffer + position buffer
4. Single instanced draw call

## Structure
```
docs/                           # Project documentation
├── Build.md                    # Build instructions
├── Controls.md                 # User controls
├── architecture.md             # System architecture
├── hierarchy.md                # Component hierarchy
└── rendergraph.md              # Render graph design
src/
├── main.cpp                    # Application entry
├── vulkan_renderer.*           # Master renderer, frame loop
├── vulkan/                     # Vulkan subsystems
│   ├── core/                   # Core Vulkan infrastructure
│   ├── pipelines/              # Pipeline management systems
│   ├── resources/              # Memory and resource management
│   ├── rendering/              # Frame graph and rendering coordination
│   ├── services/               # High-level services
│   ├── nodes/                  # Render graph nodes
│   └── monitoring/             # Performance monitoring and debugging
├── ecs/                        # ECS components and systems
│   ├── gpu_entity_manager.*    # CPU→GPU bridge
│   ├── systems/                # ECS systems
│   └── components/             # Component definitions
└── shaders/
    ├── movement_random.comp    # Compute movement
    ├── vertex.vert            # Vertex shader
    └── fragment.frag          # Fragment shader
```

## GPUEntity Structure
```cpp
struct GPUEntity {
    // Cache Line 1 (bytes 0-63) - HOT DATA: All frequently accessed in compute shaders
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, stateTimer, entityState
    glm::vec4 color;              // 16 bytes - RGBA color
    
    // Cache Line 2 (bytes 64-127) - COLD DATA: Rarely accessed
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
}; // 128 bytes total (2 cache lines optimized)
```

## Code Conventions

### Vulkan Function Call Pattern
Vulkan subsystem uses local caching pattern for VulkanFunctionLoader calls:

- **Pattern**: `const auto& vk = context->getLoader(); const VkDevice device = context->getDevice();`
- **Usage**: Apply in functions with multiple Vulkan API calls to avoid repeated `context->getLoader().vkFunction()` 
- **Files Using Pattern**: vulkan_utils.cpp, entity_compute_node.cpp, entity_graphics_node.cpp, gpu_synchronization_service.cpp, vulkan_swapchain.cpp, command_executor.cpp, vulkan_sync.cpp, descriptor_layout_manager.cpp, frame_graph.cpp, vulkan_pipeline.cpp, compute_stress_tester.cpp
- **Alternative**: VulkanManagerBase provides wrapper methods for pipeline managers

### RAII Vulkan Resource Management
Vulkan resources use RAII wrappers for automatic cleanup and exception safety:

- **Implementation**: `vulkan_raii.h/cpp` - Template-based RAII wrappers with move semantics
- **Wrappers**: ShaderModule, Semaphore, Fence, Pipeline, DescriptorSetLayout, etc.
- **Usage**: `vulkan_raii::ShaderModule shader = vulkan_raii::make_shader_module(handle, context)`
- **Destruction Order**: Explicit `cleanupBeforeContextDestruction()` methods prevent use-after-free
- **Migrated Components**: ShaderManager, VulkanSync (semaphores/fences)
- **Access Pattern**: `wrapper.get()` for raw handle, automatic cleanup on scope exit
