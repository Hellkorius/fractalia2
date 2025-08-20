# Vulkan Render Graph Nodes

## Purpose
Frame graph nodes for GPU-driven entity rendering pipeline. Implements compute-graphics hybrid architecture with 80,000+ entities at 60 FPS using instanced rendering and compute shaders for movement calculations.

## File Hierarchy
```
/home/nar/projects/fractalia2render/src/vulkan/nodes/
├── CLAUDE.md                      # This documentation  
├── changelog.md                   # Development history and gotchas
├── entity_compute_node.{h,cpp}    # GPU velocity calculation (900 frame cycles)
├── physics_compute_node.{h,cpp}   # GPU physics integration (velocity→position)
├── entity_graphics_node.{h,cpp}   # Instanced entity rendering
└── swapchain_present_node.{h,cpp} # Final presentation
```

## Core Components

### EntityComputeNode (Movement)
**Inputs:**
- `entityBufferId` - GPUEntity structs for velocity updates

**Outputs:**
- Modified velocity in entity buffer (every 900 frames)

**Data Flow:**
1. Executes `movement_random.comp` shader every 900 frames
2. Sets random velocity direction and magnitude in entity buffer
3. Entities maintain velocity until next 900-frame cycle

### PhysicsComputeNode  
**Inputs:**
- `entityBufferId` - GPUEntity structs for velocity data
- `currentPositionBufferId` - Current positions for integration

**Outputs:**
- `positionBufferId` - Updated positions for graphics
- `currentPositionBufferId` - Updated current positions

**Data Flow:**
1. Executes `physics.comp` shader every frame
2. Reads velocity from entity buffer
3. Integrates position += velocity * deltaTime with minimal damping (0.001)

### EntityGraphicsNode  
**Inputs:**
- `entityBufferId` - SoA entity data for instanced rendering
- `positionBufferId` - Computed positions from physics compute node
- `currentSwapchainImageId` - Dynamic per-frame render target (set via `setCurrentSwapchainImageId()`)
- `GraphicsPipelineManager*` - Graphics pipeline access
- `VulkanSwapchain*` - Render target management
- `ResourceContext*` - Resource allocation
- `GPUEntityManager*` - Entity buffer management
- `flecs::world*` - ECS world for camera matrix access

**Outputs:**
- Rendered entities to swapchain image
- Updated uniform buffers (view/projection matrices)
- Instanced draw calls via `vkCmdDrawIndexed()`

**Data Flow:**
1. Receives frame data via `updateFrameData()` and image index via `setImageIndex()`
2. Updates uniform buffer when dirty (`markUniformBufferDirty()` forces update)
3. Retrieves camera matrices via CameraService integration
4. Binds unified descriptor set with uniform buffer + SoA storage buffers
5. Executes instanced rendering reading positions and movement params from SoA buffers

### SwapchainPresentNode
**Inputs:**  
- `currentSwapchainImageId` - Rendered swapchain image (set via `setCurrentSwapchainImageId()`)
- `VulkanSwapchain*` - Presentation interface
- Image index via `setImageIndex(uint32_t)`

**Outputs:**
- Final frame presentation
- No GPU resources produced (terminal node)

**Data Flow:**
1. Validates image index bounds against swapchain image count
2. Executes presentation commands via swapchain interface
3. Terminal node in frame graph execution

## Integration Points

### Frame Graph Architecture
- **Base Class:** `FrameGraphNode` with `DECLARE_FRAME_GRAPH_NODE()` macro
- **Resource IDs:** `FrameGraphTypes::ResourceId` for buffer/image tracking
- **Dependencies:** `ResourceDependency` structs define pipeline synchronization
- **Queue Requirements:** Nodes specify compute vs graphics queue needs

### ECS Integration
- **GPUEntityManager:** Bridge between Flecs ECS and GPU buffers
- **GPUEntity Structure:** 128-byte cache-optimized layout (2 cache lines)
- **Component Mapping:** Transform, Renderable, MovementPattern → GPUEntity
- **Service Pattern:** Uses ServiceLocator for CameraService access

### Pipeline Integration  
- **Compute:** Uses ComputePipelineManager for movement shader dispatch
- **Graphics:** Uses GraphicsPipelineManager for instanced entity rendering
- **Synchronization:** Memory barriers between compute and graphics stages
- **Resource Context:** Manages buffer allocation and descriptor sets

## Key Conventions

### Resource Management
- **Dynamic IDs:** Swapchain images use per-frame resource IDs set externally
- **Static IDs:** Entity and position buffers have fixed resource IDs
- **External Dependencies:** All pipeline managers validated during construction
- **RAII Pattern:** No direct Vulkan handle cleanup (handled by frame graph)

### Error Handling
- **Constructor Validation:** Null pointer checks with exceptions
- **Runtime Validation:** Debug logging with atomic counters
- **Timeout Detection:** Optional GPUTimeoutDetector for compute shader monitoring
- **Bounds Checking:** Image index validation against swapchain size

### Performance Optimizations
- **Chunked Dispatch:** Adaptive workgroup chunking for large entity counts
- **Uniform Buffer Caching:** Only update when dirty flag set
- **Memory Barriers:** Minimal barriers between compute and graphics
- **Push Constants:** Direct frame data without additional buffer uploads

## Dependencies

### Peripheral Modules
- `../rendering/frame_graph.h` - Base node interface and resource types
- `../pipelines/` - Pipeline managers for compute and graphics execution
- `../core/vulkan_context.h` - Vulkan device and loader access
- `../../ecs/gpu/gpu_entity_manager.h` - CPU-GPU entity bridge
- `../../ecs/services/camera_service.h` - Camera matrix integration

### External Services
- **CameraService:** Provides view/projection matrices via ServiceLocator
- **VulkanSwapchain:** Manages presentation surface and image rotation
- **ResourceContext:** Handles buffer allocation and descriptor management

## Critical Notes
- **Update Requirement:** When referenced files change (pipelines, frame graph, ECS integration), this documentation must be updated
- **Resource ID Management:** Swapchain image IDs must be set each frame before execution
- **Queue Dependencies:** Compute node must execute before graphics node for correct synchronization
- **Entity Count Limits:** Maximum entities constrained by compute dispatch limits (adaptive chunking handles overflow)
- **Cache Optimization:** GPUEntity structure designed for GPU cache efficiency (hot/cold data separation)

---
*Last Updated: 2025-08-20 - Current implementation with service-based architecture integration*