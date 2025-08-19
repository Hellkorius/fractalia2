# Vulkan Pipelines System

## Overview
Vulkan pipeline management subsystem providing RAII-wrapped pipeline creation, caching, and management for compute and graphics workloads. Built on modular component architecture with LRU caching and hardware-aware optimizations.

## System Architecture

### Core Components
- **`PipelineSystemManager`** - Unified entry point managing all pipeline subsystems
- **`ComputePipelineManager`** - Component-based compute pipeline management
- **`GraphicsPipelineManager`** - Modular graphics pipeline management
- **`ShaderManager`** - SPIR-V module loading with hot-reload support
- **`DescriptorLayoutManager`** - Cached descriptor set layout creation

### Modular Compute Components
- **`ComputePipelineCache`** - LRU cache for compute PSOs
- **`ComputePipelineFactory`** - Pipeline compilation and creation
- **`ComputeDispatcher`** - Optimized dispatch operations with barriers
- **`ComputeDeviceInfo`** - Hardware capability queries
- **`ComputePipelineTypes`** - Shared data structures

### Modular Graphics Components
- **`GraphicsPipelineCache`** - LRU cache for graphics PSOs
- **`GraphicsPipelineFactory`** - Pipeline compilation engine
- **`GraphicsRenderPassManager`** - Render pass creation and caching
- **`GraphicsPipelineLayoutBuilder`** - Pipeline layout utilities
- **`GraphicsPipelineStateHash`** - State comparison and hashing

## Architecture

### Data Flow
```
Input: ComputePipelineState/GraphicsPipelineState
  ↓
ShaderManager (SPIR-V loading) + DescriptorLayoutManager (layouts)
  ↓
PipelineFactory (compilation) → PipelineCache (LRU storage)
  ↓
Output: VkPipeline + VkPipelineLayout
```

### Input/Output Contracts

**Input Data Structures:**
- `ComputePipelineState` - Complete compute pipeline specification with shader path, descriptor layouts, workgroup hints
- `GraphicsPipelineState` - Complete graphics state including vertex input, rasterization, blending, render pass
- `DescriptorLayoutSpec` - Binding specifications for descriptor set layout creation
- `ShaderModuleSpec` - Shader loading specification with hot-reload support

**Output Objects:**
- `VkPipeline` - Compiled Vulkan pipeline objects (cached)
- `VkPipelineLayout` - Associated pipeline layouts
- `VkDescriptorSetLayout` - Cached descriptor set layouts
- `VkRenderPass` - Render pass objects for graphics pipelines

**Dependencies:**
- **VulkanContext** - Core device, queues, function loader
- **vulkan_raii** - RAII wrappers for automatic resource cleanup

## Key Classes and Responsibilities

### PipelineSystemManager
**Responsibilities:** Unified initialization and lifecycle management for all pipeline subsystems.

**Interface:**
```cpp
bool initialize(const VulkanContext& context);
void cleanupBeforeContextDestruction();

// Component access
GraphicsPipelineManager* getGraphicsManager();
ComputePipelineManager* getComputeManager();
DescriptorLayoutManager* getLayoutManager();
ShaderManager* getShaderManager();

// Convenience methods
VkPipeline createGraphicsPipeline(const PipelineCreationInfo& info);
VkPipeline createComputePipeline(const std::string& computeShaderPath);
```

### ComputePipelineManager
**Responsibilities:** Component-coordinated compute pipeline management with caching and optimized dispatch operations.

**Internal Components:**
- `ComputePipelineCache` - LRU cache for compute PSOs
- `ComputePipelineFactory` - Pipeline compilation
- `ComputeDispatcher` - Dispatch operations with barrier management
- `ComputeDeviceInfo` - Hardware capability queries

**Input Contract:**
```cpp
struct ComputePipelineState {
    std::string shaderPath;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    uint32_t workgroupSizeX/Y/Z;  // Optimization hints
    bool isFrequentlyUsed;
};
```

**Output Contract:**
```cpp
VkPipeline getPipeline(const ComputePipelineState& state);
VkPipelineLayout getPipelineLayout(const ComputePipelineState& state);

// Optimized dispatch methods
void dispatch(VkCommandBuffer cmd, const ComputeDispatch& dispatch);
void dispatchBuffer(VkCommandBuffer cmd, const ComputePipelineState& state,
                   uint32_t elementCount, const std::vector<VkDescriptorSet>& sets);
void dispatchImage(VkCommandBuffer cmd, const ComputePipelineState& state,
                  uint32_t width, uint32_t height, const std::vector<VkDescriptorSet>& sets);
```

### GraphicsPipelineManager
**Responsibilities:** Modular graphics pipeline management with comprehensive state tracking and render pass integration.

**Internal Components:**
- `GraphicsPipelineCache` - LRU cache for graphics PSOs
- `GraphicsPipelineFactory` - Pipeline compilation engine
- `GraphicsRenderPassManager` - Render pass creation and caching
- `GraphicsPipelineLayoutBuilder` - Pipeline layout utilities
- `GraphicsPipelineStateHash` - State comparison for caching

**Input Contract:**
```cpp
struct GraphicsPipelineState {
    std::vector<std::string> shaderStages;  // Vertex, fragment, etc.
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    VkPrimitiveTopology topology;
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;
    VkSampleCountFlagBits rasterizationSamples;
    VkBool32 depthTestEnable;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    VkRenderPass renderPass;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
};
```

**Output Contract:**
```cpp
VkPipeline getPipeline(const GraphicsPipelineState& state);
VkPipelineLayout getPipelineLayout(const GraphicsPipelineState& state);
VkRenderPass createRenderPass(VkFormat colorFormat, VkFormat depthFormat,
                             VkSampleCountFlagBits samples, bool enableMSAA);

// Preset configurations
GraphicsPipelineState createDefaultState();
GraphicsPipelineState createMSAAState();
GraphicsPipelineState createWireframeState();
GraphicsPipelineState createInstancedState();
```

### ShaderManager
**Responsibilities:** SPIR-V module loading, caching, and compilation with hot-reload support.

**Input Contract:**
```cpp
struct ShaderModuleSpec {
    std::string filePath;
    ShaderSourceType sourceType;  // SPIRV_BINARY, GLSL_SOURCE, HLSL_SOURCE
    ShaderStageInfo stageInfo;     // Stage, entry point, specialization
    std::vector<std::string> includePaths;
    std::unordered_map<std::string, std::string> defines;
    bool enableHotReload;
};
```

**Output Contract:**
```cpp
VkShaderModule loadShader(const ShaderModuleSpec& spec);
VkShaderModule loadSPIRVFromFile(const std::string& filePath);
std::vector<VkShaderModule> loadShadersBatch(const std::vector<ShaderModuleSpec>& specs);

// Pipeline integration
VkPipelineShaderStageCreateInfo createShaderStage(VkShaderModule module,
                                                  VkShaderStageFlagBits stage,
                                                  const std::string& entryPoint);

// Compilation and reflection
ShaderCompilationResult compileGLSL(const std::string& source, VkShaderStageFlagBits stage);
ShaderReflection reflectShader(VkShaderModule module);
```

**Hot Reload:**
```cpp
void enableHotReload(bool enable);
void checkForShaderReloads();  // Call per frame
void registerReloadCallback(const std::string& path, std::function<void(VkShaderModule)> callback);
```

### DescriptorLayoutManager
**Responsibilities:** Cached descriptor set layout creation with optimal pool sizing and bindless support.

**Input Contract:**
```cpp
struct DescriptorLayoutSpec {
    std::vector<DescriptorBinding> bindings;
    VkDescriptorSetLayoutCreateFlags flags;
    bool enableBindless;
    bool enableUpdateAfterBind;
    bool enablePartiallyBound;
    std::string layoutName;  // Debug identifier
};

struct DescriptorBinding {
    uint32_t binding;
    VkDescriptorType type;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    bool isBindless;
    uint32_t maxBindlessDescriptors;
    std::vector<VkSampler> immutableSamplers;
};
```

**Output Contract:**
```cpp
VkDescriptorSetLayout getLayout(const DescriptorLayoutSpec& spec);
VkDescriptorSetLayout createLayout(const DescriptorLayoutSpec& spec);
std::vector<VkDescriptorSetLayout> createLayoutsBatch(const std::vector<DescriptorLayoutSpec>& specs);

// Pool management
VkDescriptorPool createOptimalPool(const DescriptorPoolConfig& config);
VkDescriptorPool createPoolForLayouts(const std::vector<VkDescriptorSetLayout>& layouts, uint32_t maxSets);

// Preset layouts
VkDescriptorSetLayout getUniformBufferLayout(VkShaderStageFlags stages);
VkDescriptorSetLayout getStorageBufferLayout(VkShaderStageFlags stages);
VkDescriptorSetLayout getBindlessTextureLayout(uint32_t maxTextures);

// Hardware capabilities
bool supportsBindless() const;
bool supportsUpdateAfterBind() const;
uint32_t getMaxBindlessDescriptors() const;
```

## Performance and Caching

### Cache Architecture
- **LRU Eviction**: All caches use least-recently-used eviction with frame-based tracking
- **Usage Tracking**: `lastUsedFrame` and `useCount` metrics for intelligent eviction
- **Size Limits**: Configurable cache sizes (compute: 512, graphics: 1024, layouts: 256, shaders: 512)
- **Statistics**: Hit ratio, compilation times, frame-based metrics

### Hardware Optimization
- **Workgroup Sizing**: `ComputeDeviceInfo` provides hardware-aware dispatch optimization
- **Bindless Support**: Hardware capability queries with fallback handling
- **RAII Resource Management**: Automatic cleanup prevents resource leaks
- **Batch Operations**: Multi-pipeline creation reduces driver call overhead

## Integration with Other Systems

### Frame Graph Integration
```cpp
// Entity compute pipeline usage in render nodes
ComputePipelineState state{
    .shaderPath = "shaders/movement_random.comp.spv",
    .descriptorSetLayouts = {entityLayout},
    .workgroupSizeX = 32
};
VkPipeline pipeline = pipelineManager->getComputeManager()->getPipeline(state);
```

### Preset System Integration
```cpp
// Compute presets for common patterns
ComputePipelineState entityMovement = ComputePipelinePresets::createEntityMovementState(descriptorLayout);
GraphicsPipelineState entityRendering = GraphicsPipelinePresets::createEntityRenderingState(renderPass, descriptorLayout);
DescriptorLayoutSpec entityLayout = DescriptorLayoutPresets::createEntityComputeLayout();
```

### Error Handling and Validation
- Pipeline state validation before compilation
- SPIR-V validation for shader modules
- Hardware capability checks for bindless features
- Graceful fallback for missing device features

### Thread Safety
- Single-threaded design for main render thread
- Async shader compilation supported via `std::future`
- Hot-reload callbacks execute on main thread