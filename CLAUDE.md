# Fractalia2 Project Documentation

## Overview
Cross-compiled (Linux→Windows) engine for 80,000+ entities at 60 FPS using GPU-driven instanced rendering.

## Stack
- SDL3, Vulkan, Flecs ECS, GLM, MinGW
- Service-based architecture with dependency injection

## Architecture

### CPU/GPU Pipeline
1. **CPU (Flecs ECS + Services)**: Entity lifecycle, input, camera, control logic
2. **GPU Compute**: Random walk movement calculation  
3. **GPU Graphics**: Instanced rendering
4. **Bridge**: GPUEntityManager + RenderingService handle CPU→GPU transfer

### Data Flow
1. Flecs components → Service layer → GPUEntity structs (128-byte layout)
2. Compute shader calculates positions with interpolation  
3. RenderingService culls and batches → Graphics reads entity buffer + position buffer
4. Optimized instanced draw calls

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
├── ecs/                        # ECS components, systems, and services
│   ├── core/                   # Core service infrastructure
│   │   ├── service_locator.*   # Service dependency injection
│   │   └── world_manager.*     # ECS world management with modules
│   ├── services/               # High-level game services
│   │   ├── input_service.*     # Advanced input with action mapping
│   │   ├── camera_service.*    # Multi-camera with transitions
│   │   ├── rendering_service.* # Render queue with culling/batching
│   │   └── control_service.*   # Game control logic coordination
│   ├── gpu_entity_manager.*    # CPU→GPU bridge
│   ├── systems/                # ECS systems (legacy, being phased out)
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

## Service Architecture

### Service Pattern
The engine uses a service-based architecture for high-level systems:

```cpp
// Service registration with dependency injection
auto worldManager = ServiceLocator::instance().createAndRegister<WorldManager>("WorldManager", 100);
auto inputService = ServiceLocator::instance().createAndRegister<InputService>("InputService", 90);

// Declare dependencies
serviceLocator.declareDependencies<InputService, WorldManager>();

// Access services
InputService& input = ServiceLocator::instance().requireService<InputService>();
// Or via convenience namespace
Input::getService().isActionJustPressed("jump");
```

### Service Lifecycle
- **Priority-based initialization**: Higher priority services initialize first
- **Dependency validation**: Ensures all dependencies are satisfied
- **Lifecycle tracking**: UNINITIALIZED → INITIALIZING → INITIALIZED → SHUTTING_DOWN → SHUTDOWN
- **Ordered cleanup**: Services shut down in reverse initialization order

### Core Services

1. **WorldManager** (Priority: 100)
   - Manages Flecs world and module loading
   - Provides ECS frame execution
   - Performance monitoring integration

2. **InputService** (Priority: 90)
   - Action-based input system with context switching
   - SDL event processing and raw input access
   - Configurable key bindings and analog inputs

3. **CameraService** (Priority: 80)
   - Multi-camera management with smooth transitions
   - Viewport support for split-screen
   - Frustum culling and coordinate transformations

4. **RenderingService** (Priority: 70)
   - Render queue management with priority sorting
   - LOD (Level of Detail) system integration
   - Frustum and occlusion culling optimization

5. **ControlService** (Priority: 60)
   - Game control logic and action coordination
   - Integration between input, camera, and rendering
   - Debug commands and performance monitoring

### Service Integration Pattern
```cpp
// Services communicate through well-defined interfaces
class ControlService {
    InputService* inputService;    // Dependency injection
    CameraService* cameraService;
    RenderingService* renderingService;
    
public:
    void processFrame(float deltaTime) {
        handleInput();      // → InputService
        updateCamera();     // → CameraService  
        optimizeRendering(); // → RenderingService
    }
};
```

## Code Conventions

### Vulkan Function Call Pattern
Vulkan subsystem uses local caching pattern for VulkanFunctionLoader calls:

- **Pattern**: `const auto& vk = context->getLoader(); const VkDevice device = context->getDevice();`
- **Usage**: Apply in functions with multiple Vulkan API calls to avoid repeated `context->getLoader().vkFunction()` 
- **Files Using Pattern**: vulkan_utils.cpp, entity_compute_node.cpp, entity_graphics_node.cpp, gpu_synchronization_service.cpp, vulkan_swapchain.cpp, command_executor.cpp, vulkan_sync.cpp, descriptor_layout_manager.cpp, frame_graph.cpp, compute_stress_tester.cpp
- **Alternative**: VulkanManagerBase provides wrapper methods for pipeline managers

### RAII Vulkan Resource Management
Vulkan resources use RAII wrappers for automatic cleanup and exception safety:

- **Implementation**: `vulkan_raii.h/cpp` - Template-based RAII wrappers with move semantics
- **Wrappers**: ShaderModule, Semaphore, Fence, Pipeline, DescriptorSetLayout, etc.
- **Usage**: `vulkan_raii::ShaderModule shader = vulkan_raii::make_shader_module(handle, context)` or `vulkan_raii::create_pipeline_cache(context, &info)`
- **Destruction Order**: Explicit `cleanupBeforeContextDestruction()` methods prevent use-after-free
- **Migrated Components**: ShaderManager, VulkanSync (semaphores/fences)
- **Access Pattern**: `wrapper.get()` for raw handle, automatic cleanup on scope exit

### Service Development Guidelines

1. **Service Definition**
   - Inherit from service pattern: `DECLARE_SERVICE(MyService)`
   - Implement initialization with clear dependencies
   - Provide cleanup and proper resource management

2. **Service Integration**
   - Use ServiceLocator for dependency access
   - Declare dependencies explicitly for validation
   - Provide convenience namespaces for global access

3. **Error Handling**
   - Return false from initialize() on failure
   - Throw exceptions for critical dependency failures
   - Log errors with context for debugging

4. **Performance**
   - Cache service references to avoid repeated lookups
   - Use lifecycle states to avoid accessing uninitialized services
   - Batch operations where possible for frame coherency

### Service Architecture Migration Notes

**Completed:**
- Full service-based architecture implementation
- Dependency injection with ServiceLocator pattern
- Service lifecycle management and cleanup ordering
- Input, Camera, Rendering, and Control services fully implemented
- Legacy system integration removed (SimpleControlSystem, direct InputManager calls)
- Convenience namespaces for backward compatibility

**Service Benefits:**
- Clean separation of concerns
- Testable and mockable dependencies
- Proper initialization order with validation
- Resource cleanup in correct order
- Thread-safe service access with lifecycle tracking
