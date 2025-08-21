# Vulkan Pipelines Subsystem

## Overview
High-performance pipeline management system for compute and graphics pipelines with advanced caching, state hashing, and optimization features. Supports both traditional and bindless descriptor layouts with hot-reloading capabilities.

## Architecture

### Core Components

**PipelineSystemManager** - Unified interface coordinating all pipeline operations
- Manages graphics, compute, shader, and descriptor layout managers
- Provides high-level pipeline creation convenience methods
- Handles system-wide operations like cache warming and recreation

**Pipeline Managers** - Specialized managers for each pipeline type
- `GraphicsPipelineManager` - Graphics pipeline state objects with render pass management
- `ComputePipelineManager` - Compute pipelines with workgroup optimization and dispatch helpers
- Both support async compilation, cache management, and performance profiling

**Resource Managers** - Supporting infrastructure
- `ShaderManager` - SPIR-V loading, GLSL compilation, hot-reloading, reflection
- `DescriptorLayoutManager` - Descriptor set layout caching with bindless support

### Data Flow

```
1. Pipeline Creation Request
   ↓
2. State Specification (GraphicsPipelineState/ComputePipelineState)
   ↓
3. Cache Lookup (by state hash)
   ↓
4. Factory Creation (if cache miss)
   → Shader Loading → Layout Creation → Pipeline Compilation
   ↓
5. Cached Pipeline Return (VkPipeline + VkPipelineLayout)
```

## Key Classes

### Pipeline State Objects
```cpp
struct GraphicsPipelineState {
    // Complete graphics state for caching
    std::vector<std::string> shaderStages;
    VkRenderPass renderPass;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    // Vertex input, rasterization, blending, etc.
};

struct ComputePipelineState {
    std::string shaderPath;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    glm::uvec3 workgroupSize;  // Optimization hints
};
```

### Caching System
- **State-based hashing** - Pipeline states generate unique hashes for cache keys
- **LRU eviction** - Least recently used pipelines are evicted when cache is full
- **Usage tracking** - Frame-based access patterns for optimization
- **Hot path detection** - Frequently used pipelines marked for priority retention

### Shader Management
```cpp
struct ShaderModuleSpec {
    std::string filePath;
    ShaderSourceType sourceType;  // SPIRV_BINARY, GLSL_SOURCE
    ShaderStageInfo stageInfo;
    bool enableHotReload;
};
```

**Features:**
- SPIR-V binary loading and GLSL compilation (via glslc)
- Shader reflection for automatic descriptor layout generation
- Hot-reloading with file system monitoring
- Specialization constants and preprocessor defines

### Descriptor Layout Management
```cpp
struct DescriptorLayoutSpec {
    std::vector<DescriptorBinding> bindings;
    bool enableBindless;
    bool enableUpdateAfterBind;
};
```

**Features:**
- Bindless texture/buffer support (up to 65K descriptors)
- Pool size calculation based on layout analysis
- Common layout presets (uniform buffers, storage buffers, combined image samplers)
- Builder pattern for complex layouts

## Essential Patterns

### Pipeline Creation
```cpp
// High-level convenience
auto& pipelineSystem = getPipelineSystemManager();
VkPipeline pipeline = pipelineSystem.createGraphicsPipeline({
    .vertexShaderPath = "vertex.spv",
    .fragmentShaderPath = "fragment.spv",
    .renderPass = renderPass,
    .enableMSAA = true
});

// Direct manager access for advanced control
auto* graphics = pipelineSystem.getGraphicsManager();
GraphicsPipelineState state = graphics->createInstancedState();
VkPipeline pipeline = graphics->getPipeline(state);
```

### Compute Dispatch Optimization
```cpp
auto* compute = pipelineSystem.getComputeManager();

// Automatic workgroup calculation
compute->dispatchBuffer(commandBuffer, state, entityCount, descriptorSets);

// Manual optimization
glm::uvec3 optimalSize = compute->calculateOptimalWorkgroupSize(dataSize);
ComputeDispatch dispatch = {
    .pipeline = pipeline,
    .groupCountX = (dataSize + optimalSize.x - 1) / optimalSize.x
};
compute->dispatch(commandBuffer, dispatch);
```

### Cache Management
```cpp
// Warmup common pipelines at startup
std::vector<GraphicsPipelineState> commonStates = {
    graphics->createDefaultState(),
    graphics->createInstancedState(),
    graphics->createMSAAState()
};
graphics->warmupCache(commonStates);

// Per-frame optimization
graphics->optimizeCache(currentFrame);
graphics->resetFrameStats();
```

### Hot Reloading
```cpp
shaderManager->enableHotReload(true);
shaderManager->registerReloadCallback("movement.comp", [](VkShaderModule newShader) {
    // Recreate dependent pipelines
    compute->reloadPipeline(computeState);
});
```

## Performance Features

**Async Compilation** - Background pipeline compilation for reduced frame time impact
**Batch Operations** - Multi-pipeline creation reduces driver overhead
**RAII Resource Management** - Automatic cleanup with `vulkan_raii` wrappers
**Statistics Tracking** - Cache hit ratios, compilation times, usage patterns
**Memory Barriers** - Optimized compute-to-compute synchronization

## Usage Notes

- Pipeline state objects are immutable - changes require new state creation
- Cache size limits prevent unbounded memory growth (configurable defaults)
- Hot reloading requires file system monitoring (development builds only)
- Bindless support requires VK_EXT_descriptor_indexing device extension
- MSAA requires explicit render pass configuration with appropriate sample counts