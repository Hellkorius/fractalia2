# /home/nar/projects/fractalia2/src/vulkan

*Complete Vulkan subsystem implementing modern frame graph architecture with 3D perspective rendering, GPU-driven compute pipelines, and comprehensive resource management for 80,000+ entities at 60 FPS.*

## Architecture Overview

**3D Rendering Pipeline**: The Vulkan subsystem has migrated from 2D orthographic to full 3D perspective rendering with depth testing, supporting complex 3D entity positioning and camera controls.

**Frame Graph Coordination**: Declarative frame graph with automatic dependency analysis, barrier insertion, and resource lifecycle management for optimal GPU performance.

**Structure of Arrays (SoA)**: Vulkan 1.3 descriptor indexing enables bindless GPU access to entityBuffers[] array, providing cache-optimized memory layout for massive entity counts.

**RAII Resource Management**: Exception-safe Vulkan object lifecycle with automatic cleanup ordering and comprehensive memory management.

```
src/vulkan/
├── core/                           # Foundational Vulkan infrastructure
│   ├── vulkan_context.*           # Central device/queue context with function loading
│   ├── vulkan_swapchain.*         # 3D-capable swapchain with depth buffer support
│   ├── vulkan_raii.*              # Template-based RAII wrappers
│   ├── vulkan_sync.*              # Synchronization primitives for async compute
│   ├── vulkan_utils.*             # Core utilities and error handling
│   └── queue_manager.*            # Centralized queue access with fallbacks
├── monitoring/                     # GPU performance monitoring
│   ├── compute_stress_tester.*    # Compute pipeline validation
│   ├── gpu_memory_monitor.*       # Memory usage tracking
│   └── gpu_timeout_detector.*     # Timeout and hang detection
├── nodes/                          # Frame graph node implementations  
│   ├── base_compute_node.*        # Template method pattern eliminating duplication
│   ├── entity_compute_node.*      # 3D movement calculation with random walk
│   ├── entity_graphics_node.*     # 3D perspective rendering with depth testing
│   ├── physics_compute_node.*     # Physics simulation and interpolation
│   └── swapchain_present_node.*   # Presentation with error recovery
├── pipelines/                      # Sophisticated pipeline management
│   ├── compute_pipeline_*         # Compute pipeline creation and caching
│   ├── graphics_pipeline_*        # 3D graphics pipeline with depth support
│   ├── descriptor_layout_*        # Vulkan 1.3 indexed layout management
│   └── shader_manager.*           # Shader compilation and hot-reload
├── rendering/                      # Frame graph coordination
│   ├── frame_graph.*              # Declarative rendering with dependency analysis
│   ├── compilation/               # Dependency graph and topological sorting
│   ├── execution/                 # Memory barrier management and optimization
│   └── resources/                 # Resource lifecycle and memory pressure handling
├── resources/                      # Comprehensive resource management
│   ├── buffers/                   # Buffer lifecycle with staging and statistics
│   ├── core/                      # Resource coordination and validation
│   ├── descriptors/               # Descriptor set management and batching
│   └── managers/                  # High-level graphics resource coordination
└── services/                       # High-level Vulkan services
    ├── render_frame_director.*    # Frame execution coordination
    ├── gpu_synchronization_*      # Async compute-graphics synchronization
    ├── error_recovery_service.*   # Error handling and recovery
    └── presentation_surface.*     # Surface management and swapchain coordination
```

## 3D Rendering Migration

### Core 3D Features

**Depth Buffer Support**: VulkanSwapchain now creates and manages depth resources with optimal format selection (D32_SFLOAT > D32_SFLOAT_S8_UINT > D24_UNORM_S8_UINT).

**3D Perspective Projection**: Camera system supports full 3D perspective matrices with configurable FOV, aspect ratio, and near/far planes.

**Z-Testing and Depth Attachment**: Graphics pipeline enables depth testing with proper render pass configuration for 3D entity ordering.

**3D Entity Positioning**: Movement centers buffer provides 3D coordinate support with spherical coordinate generation for realistic spatial distribution.

**3D Frustum Culling**: Camera service implements 3D visibility testing and frustum culling optimization for large entity counts.

### Key Implementation Changes

#### VulkanSwapchain (3D Depth Support)
```cpp
// Depth buffer creation with format selection
VkFormat findDepthFormat();
bool createDepthResources();

// 3D-capable framebuffer with depth attachment  
std::array<VkImageView, 3> attachments = {
    msaaColorImageView.get(),      
    swapChainImageViews[i].get(),
    depthImageView.get()  // Depth attachment for 3D
};
```

#### Vertex Shader (3D Processing)
```glsl
// 3D entity positioning from movement centers buffer
vec3 worldPos = entityBuffers[POSITION_OUTPUT_BUFFER].data[gl_InstanceIndex].xyz;

// 3D vertex transformation with depth preservation
vec3 rotatedVertex = vec3(
    inPos.x * cosRot - inPos.y * sinRot,
    inPos.x * sinRot + inPos.y * cosRot,
    inPos.z  // Preserve Z component for 3D geometry
);

// Full 3D transformation pipeline
vec3 finalPos = worldPos + rotatedVertex;
gl_Position = ubo.proj * ubo.view * vec4(finalPos, 1.0);
```

#### Camera System (3D Perspective)
```cpp
// 3D perspective projection matrix
projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
projectionMatrix[1][1] *= -1; // Flip Y for Vulkan

// 3D lookAt view matrix
viewMatrix = glm::lookAt(position, target, up);
```

#### Movement Computation (3D Spherical Coordinates)
```glsl
// 3D random movement with spherical coordinates
float elevation = (hashToFloat(angleHash2) - 0.5) * 1.5; // Vertical angle
float cosElev = cos(elevation);

// Generate 3D velocity vector
velocity.x = speed * cosElev * cos(randAngle + movementAngularVel);
velocity.y = speed * cosElev * sin(randAngle + movementAngularVel);  
velocity.z = speed * sin(elevation); // Vertical movement component
```

### Frame Graph Integration

**3D Depth Dependency**: Entity graphics node declares depth attachment as output dependency for proper barrier management.

**Resource Lifecycle**: Depth buffer participates in frame graph resource tracking with automatic recreation during swapchain resize.

**Memory Barriers**: Barrier manager handles depth buffer transitions between compute and graphics stages.

## Performance Optimizations

### Structure of Arrays (SoA) Layout
**Cache Optimization**: 128-byte entity layout optimized for GPU cache lines with hot/cold data separation.

**Bindless Access**: Vulkan 1.3 descriptor indexing eliminates descriptor set switching overhead.

**Memory Bandwidth**: SoA layout reduces memory bandwidth by 20-30% compared to Array of Structures.

### Async Compute-Graphics Pipeline
**Overlapped Execution**: Movement compute runs asynchronously with graphics rendering using ping-pong buffers.

**Memory Barriers**: Automatic barrier insertion ensures data consistency between compute writes and graphics reads.

**Queue Utilization**: QueueManager provides optimal queue family selection with async compute support.

### Resource Management
**RAII Lifecycle**: All Vulkan objects use RAII wrappers for exception-safe cleanup and memory leak prevention.

**Buffer Pooling**: Staging buffer pool minimizes allocation overhead for frequent transfers.

**Pipeline Caching**: Comprehensive pipeline caching with hot-reload support for development workflows.

## 3D Entity System Integration

### GPU Entity Manager
**SoA Buffer Layout**: Maintains separate buffers for velocity, movement params, movement centers, rotation state, and color data.

**3D Movement Centers**: New buffer provides 3D coordinate origins for entity spatial distribution.

**Descriptor Management**: EntityDescriptorManager handles Vulkan 1.3 indexed descriptor sets with unified binding layout.

### Camera Service Integration  
**3D View Matrices**: Camera service provides proper 3D view/projection matrices to graphics pipeline.

**Frustum Culling**: Camera service implements 3D frustum culling for off-screen entity elimination.

**Coordinate Transforms**: Full 3D coordinate system support with proper depth ordering and perspective correction.

### Physics Integration
**3D Position Output**: Physics compute shader outputs full 3D positions to position buffer.

**Interpolation Support**: Smooth 3D movement interpolation between compute updates for 60 FPS rendering.

**Collision Detection**: Spatial map buffer supports 3D spatial partitioning for efficient collision queries.

## Error Handling and Monitoring

### GPU Monitoring
**Compute Stress Testing**: Validates compute pipelines under load with timeout detection.

**Memory Monitoring**: Tracks GPU memory usage and detects memory pressure conditions.

**Performance Telemetry**: Comprehensive statistics for draw calls, buffer updates, and pipeline switches.

### Error Recovery  
**Swapchain Recreation**: Automatic swapchain recreation on resize with proper resource cleanup.

**Pipeline Recovery**: Hot-reload support for shader development with fallback pipeline management.

**Validation Integration**: Validation layer integration with detailed error reporting and stack traces.

## Development Tools

### Frame Graph Debug
**Resource Tracking**: Visual debugging for resource dependencies and barrier insertion.

**Performance Analysis**: Detailed timing and memory usage analysis per frame graph node.

**Validation Mode**: Debug-only validation with comprehensive state checking and leak detection.

### Shader Development
**Hot Reload**: Live shader recompilation during development without application restart.

**Error Reporting**: Detailed compilation error reporting with source line mapping.

**Performance Profiling**: Per-shader performance metrics and GPU utilization analysis.