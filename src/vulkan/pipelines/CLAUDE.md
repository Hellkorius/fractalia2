# Vulkan Pipelines System

## Overview
AAA-tier Vulkan pipeline management system with advanced caching, optimization, and hot-reload capabilities for compute and graphics pipelines.

## File Hierarchy

### Core Managers
- **`pipeline_system_manager.*`** - Unified interface for all pipeline operations
- **`compute_pipeline_manager.*`** - Compute pipeline caching & dispatch optimization  
- **`graphics_pipeline_manager.*`** - Refactored modular graphics pipeline coordinator
- **`shader_manager.*`** - SPIR-V loading, compilation & hot-reload support
- **`descriptor_layout_manager.*`** - Descriptor set layout caching & pool management

### Graphics Pipeline Components (New Modular Architecture)
- **`graphics_pipeline_state_hash.*`** - Pipeline state comparison and hashing utilities
- **`graphics_pipeline_cache.*`** - LRU cache with statistics tracking
- **`graphics_render_pass_manager.*`** - Render pass creation and caching
- **`graphics_pipeline_factory.*`** - Core pipeline compilation engine
- **`graphics_pipeline_layout_builder.*`** - Pipeline layout creation utilities

### Utilities  
- **`hash_utils.*`** - Pipeline state hashing and comparison utilities

## Architecture

### Data Flow
```
ShaderManager → DescriptorLayoutManager → PipelineManagers → PipelineSystemManager
      ↓                    ↓                       ↓                    ↓
   SPIR-V Code      Descriptor Layouts      Pipeline Objects      Unified API
```

### Dependencies
- **VulkanContext**: Core Vulkan objects (device, queues, etc.)
- **External Tools**: `glslc` for GLSL→SPIR-V compilation
- **Frame Graph**: Integration with rendering pipeline

## API Reference

### PipelineSystemManager (Entry Point)

**Initialization:**
```cpp
PipelineSystemManager manager;
manager.initialize(vulkanContext);

// Access individual managers
auto* graphics = manager.getGraphicsManager();
auto* compute = manager.getComputeManager();  
auto* layouts = manager.getLayoutManager();
auto* shaders = manager.getShaderManager();
```

**High-Level Pipeline Creation:**
```cpp
PipelineCreationInfo info{
    .vertexShaderPath = "shaders/vertex.spv",
    .fragmentShaderPath = "shaders/fragment.spv", 
    .renderPass = renderPass,
    .enableMSAA = true,
    .samples = VK_SAMPLE_COUNT_2_BIT
};
VkPipeline pipeline = manager.createGraphicsPipeline(info);
```

### ComputePipelineManager (Refactored Architecture)

**Architecture:** Refactored from monolithic 682-line class into focused components:
- **ComputePipelineCache**: LRU caching with statistics (95 lines)
- **ComputePipelineFactory**: Pipeline creation and compilation (118 lines)
- **ComputeDispatcher**: Dispatch operations and barriers (104 lines)
- **ComputeDeviceInfo**: Device capabilities and optimization (58 lines)
- **ComputePipelineTypes**: Shared data structures and utilities

**Input:** 
- `ComputePipelineState` - Complete pipeline specification
- `ComputeDispatch` - Dispatch parameters with optimization hints

**Output:**
- `VkPipeline` - Cached compute pipeline via coordinated components
- `VkPipelineLayout` - Associated pipeline layout via factory
- Performance metrics & statistics via aggregated component data

**Key APIs:**
```cpp
// Pipeline creation with caching (unchanged interface)
ComputePipelineState state{
    .shaderPath = "shaders/movement.comp.spv",
    .descriptorSetLayouts = {entityLayout},
    .pushConstantRanges = {{VK_SHADER_STAGE_COMPUTE_BIT, 0, 32}},
    .workgroupSizeX = 32
};
VkPipeline pipeline = manager.getPipeline(state);

// Optimized dispatch (delegated to ComputeDispatcher)
manager.dispatch(commandBuffer, dispatch);
manager.dispatchBuffer(commandBuffer, state, entityCount, descriptorSets);
manager.dispatchIndirect(commandBuffer, buffer, offset);

// Component access for advanced usage
auto* cache = manager.getCache();
auto* deviceInfo = manager.getDeviceInfo();
```

**Performance Features:**
- Component-based LRU cache with configurable size (default: 512 pipelines)
- Async compilation for hot-reload
- Hardware-aware workgroup optimization via ComputeDeviceInfo
- Centralized dispatch operations with barrier optimization

### GraphicsPipelineManager (Refactored Modular Architecture)

**Architecture**: Refactored from monolithic 769-line class into focused components:
- **GraphicsPipelineCache**: LRU caching with statistics (was inline cache logic)
- **GraphicsRenderPassManager**: Render pass creation/caching (was 118-line method)
- **GraphicsPipelineFactory**: Core compilation engine (was 182-line method)
- **GraphicsPipelineLayoutBuilder**: Layout creation utilities (was inline logic)
- **GraphicsPipelineStateHash**: State comparison and hashing (was complex operators)

**Input:**
- `GraphicsPipelineState` - Complete graphics pipeline specification
- Render pass, vertex input, rasterization, blending state

**Output:**  
- `VkPipeline` - Cached graphics pipeline via coordinated components
- `VkPipelineLayout` - Associated pipeline layout via layout builder
- `VkRenderPass` - Render pass objects via dedicated manager

**Key APIs:**
```cpp
// Create graphics pipeline state
GraphicsPipelineState state = GraphicsPipelinePresets::createEntityRenderingState(
    renderPass, descriptorLayout);
state.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;
state.cullMode = VK_CULL_MODE_BACK_BIT;

VkPipeline pipeline = manager.getPipeline(state);

// Render pass creation
VkRenderPass renderPass = manager.createRenderPass(
    VK_FORMAT_B8G8R8A8_UNORM,     // Color format
    VK_FORMAT_D32_SFLOAT,         // Depth format  
    VK_SAMPLE_COUNT_2_BIT,        // MSAA samples
    true                          // Enable MSAA
);
```

**Preset Configurations:**
- `createDefaultState()` - Basic 3D rendering
- `createMSAAState()` - Multi-sample anti-aliasing
- `createWireframeState()` - Debug wireframe rendering
- `createInstancedState()` - Instanced rendering optimized

### ShaderManager

**Input:**
- `ShaderModuleSpec` - Shader file path, stage, specialization constants
- GLSL source code or pre-compiled SPIR-V

**Output:**
- `VkShaderModule` - Compiled Vulkan shader module
- `ShaderReflection` - Descriptor bindings, push constants analysis
- `VkPipelineShaderStageCreateInfo` - Ready-to-use pipeline stage

**Key APIs:**
```cpp
// Load pre-compiled SPIR-V
VkShaderModule shader = manager.loadSPIRVFromFile("shaders/compute.spv");

// Load with specification
ShaderModuleSpec spec{
    .filePath = "shaders/movement.comp.spv",
    .sourceType = ShaderSourceType::SPIRV_BINARY,
    .stageInfo = {.stage = VK_SHADER_STAGE_COMPUTE_BIT},
    .enableHotReload = true
};
VkShaderModule shader = manager.loadShader(spec);

// GLSL compilation (requires glslc)
auto result = manager.compileGLSLFromFile("shaders/vertex.glsl");
if (result.success) {
    VkShaderModule shader = manager.createVulkanShaderModule(result.spirvCode);
}

// Hot reload support
manager.enableHotReload(true);
manager.registerReloadCallback("shaders/fragment.glsl", 
    [](VkShaderModule newShader) { /* Update pipeline */ });
```

**Reflection Capabilities:**
```cpp
ShaderReflection reflection = manager.reflectShader(shaderModule);
// Access: descriptorBindings, pushConstantRanges, localSize (compute)
```

### DescriptorLayoutManager

**Input:**
- `DescriptorLayoutSpec` - Binding specifications, flags, bindless configuration
- `DescriptorBinding` - Individual binding description

**Output:**
- `VkDescriptorSetLayout` - Cached descriptor set layout
- `VkDescriptorPool` - Optimally sized descriptor pools
- Pool size calculations based on usage patterns

**Key APIs:**
```cpp
// Create layout specification
DescriptorLayoutSpec spec{
    .bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}
    },
    .layoutName = "EntityComputeLayout"
};
VkDescriptorSetLayout layout = manager.getLayout(spec);

// Builder pattern for complex layouts
DescriptorLayoutBuilder builder;
VkDescriptorSetLayout layout = builder
    .addUniformBuffer(0, VK_SHADER_STAGE_ALL)
    .addStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT)
    .addCombinedImageSampler(2, 4, VK_SHADER_STAGE_FRAGMENT_BIT)
    .setName("MaterialLayout")
    .build();

// Preset layouts
VkDescriptorSetLayout entityLayout = DescriptorLayoutPresets::createEntityComputeLayout();
VkDescriptorSetLayout bindlessLayout = manager.getBindlessTextureLayout(16384);

// Optimal pool creation
VkDescriptorPool pool = manager.createPoolForLayouts({layout1, layout2}, 1024);
```

**Bindless Support:**
- Update-after-bind descriptors
- Partially bound descriptor sets  
- Hardware capability queries

## Performance Considerations

### Caching Strategy
- **Pipeline Cache**: LRU eviction, frame-based cleanup
- **Layout Cache**: Persistent, small memory footprint
- **Shader Cache**: Hot-reload aware, modification time tracking

### Optimization Features
- **Batch Operations**: Reduce driver overhead via batch creation
- **Workgroup Sizing**: Hardware-aware compute dispatch optimization
- **Memory Barriers**: Minimal barrier insertion with conflict detection
- **Pool Sizing**: Usage-based descriptor pool size calculation

### Memory Management
- **Cache Limits**: Configurable max sizes (pipelines: 512-1024, layouts: 256, shaders: 512)
- **Cleanup Intervals**: Frame-based eviction (default: 1000 frames)
- **Pool Strategy**: Pre-sized pools based on layout analysis

## Integration Points

### Frame Graph Integration
```cpp
// Entity compute node usage
ComputePipelineState movementState = ComputePipelinePresets::createEntityMovementState(descriptorLayout);
VkPipeline movementPipeline = computeManager->getPipeline(movementState);
computeManager->dispatchBuffer(cmd, movementState, entityCount, descriptorSets);
```

### Hot Reload Workflow
```cpp
shaderManager->enableHotReload(true);
shaderManager->checkForShaderReloads();  // Call per frame
// Automatic pipeline invalidation and recreation
```

### Statistics & Debugging
```cpp
// Performance monitoring
auto stats = pipelineManager->getStats();
std::cout << "Cache hit ratio: " << stats.hitRatio << std::endl;
std::cout << "Compilations this frame: " << stats.compilationsThisFrame << std::endl;

// Debug output
manager.debugPrintCache();
```

## Error Handling

### Validation
- Pipeline state validation before creation
- Shader SPIR-V validation
- Descriptor layout compatibility checks
- Hardware capability verification

### Fallbacks
- Graceful degradation for missing features
- Legacy pipeline system compatibility
- Default state creation for common use cases

## Thread Safety

### Concurrent Access
- **Read Operations**: Thread-safe (const methods)
- **Write Operations**: Single-threaded (main thread only)
- **Cache Access**: Protected by internal synchronization

### Async Compilation
- Background shader compilation supported
- Future-based async pipeline creation
- Hot-reload callbacks executed on main thread