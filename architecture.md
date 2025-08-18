# Fractalia2 Engine Architecture

## Technology Stack
- **Graphics**: Vulkan 1.3 with frame graph
- **ECS**: Flecs entity component system
- **Platform**: SDL3
- **Math**: GLM
- **Build**: MinGW cross-compilation

## Modular Services

**RenderFrameDirector** (`src/vulkan/render_frame_director.*`)
- Frame execution: acquire → setup → compile → configure → execute → submit
- Node configuration with stored IDs and world reference
- `directFrame()` main execution method

**FrameGraph** (`src/vulkan/frame_graph.*`)
- Resource dependency management
- Compile-once execution model
- Automatic barrier insertion for compute→graphics transitions
- External resource import

**CommandSubmissionService** (`src/vulkan/command_submission_service.*`)
- GPU queue submission
- Multi-frame synchronization
- Fence/semaphore management

**FrameGraphResourceRegistry** (`src/vulkan/frame_graph_resource_registry.*`)
- Entity/position buffer registration and ID mapping
- Resource import coordination

**GPUSynchronizationService** (`src/vulkan/gpu_synchronization_service.*`)
- Per-frame fence management
- Compute/graphics synchronization

**PresentationSurface** (`src/vulkan/presentation_surface.*`)
- Swapchain lifecycle
- Surface acquisition and presentation

## Data Flow

```
CPU (Flecs ECS) → GPU Compute (Random Walk) → GPU Graphics (Instanced Rendering)
```

### Memory Layout
```cpp
struct GPUEntity {
    glm::vec4 movementParams0;    // amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // center.xyz, movementType  
    glm::vec4 runtimeState;       // totalTime, initialized, stateTimer, entityState
    glm::vec4 color;              // RGBA color
    glm::mat4 modelMatrix;        // transform matrix
}; // 128 bytes (2 cache lines)
```

## Node Architecture
- **EntityComputeNode**: Random walk movement, 600-frame interpolation cycles
- **EntityGraphicsNode**: Instanced rendering with camera matrix integration
- **SwapchainPresentNode**: Presentation with proper image indexing

## Fixes - January 2025

### Frame Graph Compilation Loop
- **Issue**: `reset()` clearing `compiled` flag every frame
- **Fix**: Preserve `compiled` state after first compilation
- **Files**: `src/vulkan/frame_graph.cpp:349`

### Frame Synchronization
- **Issue**: Frame counter stuck at 0
- **Fix**: Separate `frameCounter` (compute) vs `currentFrame` (buffer sync)
- **Files**: `src/vulkan/frame_graph.cpp:222`, `src/vulkan/render_frame_director.cpp:221`

### Camera-Viewport Integration
- **Issue**: World reference timing
- **Fix**: Configure nodes between acquisition and execution
- **Files**: `src/vulkan/render_frame_director.cpp:75`

### Node Configuration
- **Issue**: Hard-coded node IDs
- **Fix**: Store actual node IDs from `addNode()`
- **Files**: `src/vulkan/render_frame_director.h:86-88`, `src/vulkan/render_frame_director.cpp:147-174`

## Performance
- **Entities**: 80,000 at 60 FPS
- **Frame Time**: 16.67ms consistent
- **Workgroups**: 2,500 workgroups (32 threads each)
- **Memory**: Cache-optimized 128-byte entity layout
- **Movement**: 600-frame interpolated cycles with staggering

## Performance Optimizations - January 2025

### Barrier System O(n) Optimization
- **Previous**: Quadratic complexity barrier insertion
- **Current**: Linear dependency tracking with resource state cache
- **Files**: `src/vulkan/frame_graph.cpp:597-643`

### Uniform Buffer Dirty Tracking
- **Previous**: 60 FPS memcpy for camera matrices
- **Current**: Matrix comparison with dirty flags
- **Files**: `src/vulkan/nodes/entity_graphics_node.h:74-81`, `src/vulkan/nodes/entity_graphics_node.cpp:168-240`

### Command Buffer Pooling
- **Previous**: Individual buffer resets
- **Current**: Batch reset via `vkResetCommandPool`
- **Files**: `src/vulkan/vulkan_sync.h:26-28`, `src/vulkan/vulkan_sync.cpp:122-151`

## AAA Pipeline System Architecture

### PipelineSystemManager
**Unified interface for all pipeline operations**
- Initializes and manages specialized pipeline managers in dependency order
- Provides high-level convenience methods for pipeline creation
- Aggregates statistics and performance metrics across all managers
- Legacy compatibility layer for gradual migration

### ComputePipelineManager
**Advanced compute pipeline caching with dispatch optimization**
- PSO (Pipeline State Object) caching with hash-based lookup
- Workgroup size optimization and dispatch configuration
- Async compilation support for hot reloading
- Specialized dispatch methods: `dispatchBuffer()`, `dispatchImage()`
- Memory barrier optimization and GPU timeline management
- LRU cache eviction (1000-frame threshold)

### GraphicsPipelineManager  
**Graphics pipeline state objects with comprehensive caching**
- Complete PSO specification: vertex input, rasterization, multisampling, blending
- Render pass creation and management with MSAA support
- Batch pipeline creation for reduced driver overhead
- Hot-reload support for development workflows
- Dynamic state management (viewport, scissor)

### ShaderManager
**SPIR-V loading, compilation, and hot-reload system**
- File-based shader loading with automatic stage detection
- GLSL compilation integration (via external glslc)
- Hot-reload with file system watching
- Shader reflection for descriptor layout generation
- Cache management with LRU eviction
- Debug and validation support

### DescriptorLayoutManager
**Descriptor set layout caching and pool management**
- Layout specification with bindless support
- Automatic pool sizing based on usage patterns
- Device capability queries (bindless, update-after-bind)
- Builder pattern for complex layouts
- Common layout presets (entity graphics/compute)

### Pipeline Creation Flow
1. **ShaderManager** loads and caches SPIR-V modules
2. **DescriptorLayoutManager** creates cached descriptor layouts
3. **GraphicsPipelineManager/ComputePipelineManager** creates PSOs
4. **PipelineSystemManager** coordinates the process with convenience methods

### Cache Architecture
- **Hash-based caching**: All managers use hash-based PSO/layout lookup
- **LRU eviction**: 1000-frame threshold for unused pipeline cleanup
- **Statistics tracking**: Hit ratios, compilation times, cache sizes
- **Memory barriers**: Optimized barrier insertion for compute→graphics

### Performance Features
- **Batch compilation**: Reduced driver overhead via batch operations
- **Async compilation**: Background compilation with futures
- **Workgroup optimization**: Device-specific optimal sizes
- **Render pass caching**: Hash-based render pass reuse
- **Descriptor pool sizing**: Automatic pool configuration

## Extension Points
- Multi-pass rendering via frame graph nodes
- Post-processing shader chains  
- Shadow mapping with additional depth passes
- Async compute with multi-queue scheduling
- Resource streaming via external import system
- Pipeline variants with specialization constants
- Bindless texture/buffer management
- GPU-driven rendering with indirect draws