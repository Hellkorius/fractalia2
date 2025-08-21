# Vulkan Frame Graph Nodes

## Purpose
Frame graph nodes implement discrete stages of the GPU-driven rendering pipeline. Each node encapsulates a specific computational unit (compute shader, graphics pass, or presentation) with explicit resource dependencies and queue requirements.

## Architecture

### Node Base Pattern
All nodes inherit from `FrameGraphNode` and implement:
- **Resource Dependencies**: Explicit input/output resource declarations with access patterns
- **Queue Requirements**: Compute vs graphics queue specification  
- **Lifecycle Management**: Initialize → PrepareFrame → Execute → ReleaseFrame
- **Execution Context**: Command buffer recording with frame graph resource access

### Core Node Types

#### EntityComputeNode
- **Purpose**: GPU entity movement computation using compute shaders
- **Queue**: Compute queue with chunked dispatch for stability
- **Resources**: Entity buffer (R/W), position buffers (current/target)
- **Pattern**: Chunked workgroup dispatch with adaptive sizing for 80K+ entities
- **Dependencies**: ComputePipelineManager, GPUEntityManager, GPUTimeoutDetector

#### EntityGraphicsNode  
- **Purpose**: Instanced entity rendering with frustum culling
- **Queue**: Graphics queue with swapchain integration
- **Resources**: Entity buffer (R), position buffer (R), swapchain images (W)
- **Pattern**: Camera matrix UBO caching with dirty tracking
- **Dependencies**: GraphicsPipelineManager, VulkanSwapchain, ResourceCoordinator

#### PhysicsComputeNode
- **Purpose**: Physics simulation compute pass (parallel to entity movement)
- **Queue**: Compute queue with chunked dispatch
- **Resources**: Same as EntityComputeNode (different shader binding)
- **Pattern**: Identical dispatch strategy, separate physics calculations

#### SwapchainPresentNode
- **Purpose**: Final presentation to swapchain images
- **Queue**: Graphics queue for presentation operations
- **Resources**: Color target (R), swapchain images (dynamic per-frame)
- **Pattern**: Simple pass-through with image transition handling

## Data Flow
```
CPU (ECS) → GPU Buffers → Compute Nodes → Graphics Node → Present Node → Display
           ↑                    ↓              ↓              ↓
    GPUEntityManager    Entity Movement   Entity Rendering   Swapchain
```

## Key Patterns

### Resource Dependency Declaration
```cpp
std::vector<ResourceDependency> getInputs() const override {
    return {{entityBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader}};
}
```

### Chunked Compute Dispatch
- **Workgroup Chunking**: Splits large entity counts into manageable chunks
- **Adaptive Sizing**: Dynamic workgroup limits based on GPU capabilities
- **Push Constants**: Frame timing and entity offset data per chunk

### Lifecycle Integration
- **initializeNode()**: One-time setup with frame graph validation
- **prepareFrame()**: Per-frame data preparation with timing parameters
- **execute()**: Command buffer recording with resource access
- **releaseFrame()**: Per-frame cleanup and state reset

### Queue Management
- Compute nodes specify `needsComputeQueue() = true`
- Graphics/present nodes use graphics queue
- Frame graph handles queue family transitions automatically

## Performance Considerations
- **Chunked Dispatch**: Prevents GPU timeouts with large entity counts
- **Resource Barriers**: Automatic synchronization between node stages
- **UBO Caching**: Graphics node caches camera matrices with dirty tracking
- **Atomic Counters**: Thread-safe debug statistics without locks