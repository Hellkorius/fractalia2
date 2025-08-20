# Vulkan Pipeline Management System

## Purpose
Comprehensive Vulkan pipeline infrastructure providing RAII-wrapped creation, intelligent caching, and hardware-optimized management for compute and graphics workloads. Designed for high-performance real-time applications with 80,000+ entities.

## File Hierarchy
```
pipelines/
├── pipeline_system_manager.{h,cpp}       # Unified pipeline system entry point
├── compute_pipeline_manager.{h,cpp}      # Compute pipeline management
├── graphics_pipeline_manager.{h,cpp}     # Graphics pipeline management  
├── shader_manager.{h,cpp}                # SPIR-V loading, compilation, hot-reload
├── descriptor_layout_manager.{h,cpp}     # Descriptor set layout creation & caching
├── compute_pipeline_cache.{h,cpp}        # LRU cache for compute PSOs
├── graphics_pipeline_cache.{h,cpp}       # LRU cache for graphics PSOs
├── compute_pipeline_factory.{h,cpp}      # Compute pipeline compilation engine
├── graphics_pipeline_factory.{h,cpp}     # Graphics pipeline compilation engine
├── graphics_render_pass_manager.{h,cpp}  # Render pass creation & management
├── graphics_pipeline_layout_builder.{h,cpp}  # Pipeline layout utilities
├── graphics_pipeline_state_hash.{h,cpp}  # Graphics state hashing for cache
├── compute_pipeline_types.{h,cpp}        # Compute pipeline data structures
├── compute_dispatcher.{h,cpp}            # Optimized compute dispatch operations
├── compute_device_info.{h,cpp}           # Hardware capability queries
├── pipeline_utils.{h,cpp}                # Common pipeline utilities
└── hash_utils.h                          # Hash combination utilities
```

## Core Input/Output Contracts

### PipelineSystemManager
**Inputs:**
- `VulkanContext&` - Core Vulkan device, queues, function loader
- `PipelineCreationInfo` - High-level pipeline specifications

**Outputs:**
- `VkPipeline` handles for compute/graphics pipelines
- Component access: `getGraphicsManager()`, `getComputeManager()`, `getShaderManager()`, `getLayoutManager()`
- Unified lifecycle management and statistics aggregation

### ComputePipelineManager
**Inputs:**
- `ComputePipelineState` - Complete compute specification:
  ```cpp
  struct ComputePipelineState {
      std::string shaderPath;                        // SPIR-V shader file
      std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
      std::vector<VkPushConstantRange> pushConstantRanges;
      uint32_t workgroupSize{X,Y,Z};                // Hardware optimization hints
      bool isFrequentlyUsed;                        // Cache priority
  };
  ```
- `ComputeDispatch` - Dispatch parameters with barriers and descriptor sets

**Outputs:**
- `VkPipeline` and `VkPipelineLayout` for bound compute operations
- Optimized dispatch methods: `dispatchBuffer()`, `dispatchImage()`, `dispatch()`
- Hardware-aware workgroup size calculations
- Performance profiling data and cache statistics

### GraphicsPipelineManager  
**Inputs:**
- `GraphicsPipelineState` - Complete graphics specification:
  ```cpp
  struct GraphicsPipelineState {
      std::vector<std::string> shaderStages;        // Vertex, fragment paths
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
  };
  ```

**Outputs:**  
- `VkPipeline` and `VkPipelineLayout` for graphics rendering
- `VkRenderPass` creation with MSAA and depth support
- Preset configurations: `createDefaultState()`, `createMSAAState()`, `createWireframeState()`

### ShaderManager
**Inputs:**
- `ShaderModuleSpec` - Shader loading specification:
  ```cpp
  struct ShaderModuleSpec {
      std::string filePath;                         // Shader file path
      ShaderSourceType sourceType;                 // SPIRV_BINARY, GLSL_SOURCE
      ShaderStageInfo stageInfo;                    // Stage, entry point, specialization
      std::vector<std::string> includePaths;       // GLSL includes
      std::unordered_map<std::string, std::string> defines;  // Preprocessor defines
      bool enableHotReload;                         // Development hot-reload
  };
  ```

**Outputs:**
- `VkShaderModule` handles with automatic cleanup
- `VkPipelineShaderStageCreateInfo` for pipeline creation
- `ShaderReflection` data for descriptor layout generation
- Hot-reload callbacks and file system monitoring

### DescriptorLayoutManager
**Inputs:**
- `DescriptorLayoutSpec` - Layout specification:
  ```cpp
  struct DescriptorLayoutSpec {
      std::vector<DescriptorBinding> bindings;      // Binding descriptions
      VkDescriptorSetLayoutCreateFlags flags;
      bool enableBindless;                          // Bindless resource support
      bool enableUpdateAfterBind;                   // Dynamic descriptor updates
      std::string layoutName;                       // Debug identifier
  };
  ```

**Outputs:**
- `VkDescriptorSetLayout` with intelligent caching
- `VkDescriptorPool` with optimal sizing based on usage patterns
- Bindless texture/buffer layouts for modern GPU features
- Hardware capability queries for feature support

## Data Flow Architecture

```
ShaderModuleSpec → ShaderManager → VkShaderModule
                                     ↓
DescriptorLayoutSpec → DescriptorLayoutManager → VkDescriptorSetLayout
                                     ↓
{Compute|Graphics}PipelineState → PipelineFactory → VkPipeline + VkPipelineLayout
                                     ↓
                              PipelineCache (LRU)
                                     ↓
               Frame Graph Nodes (EntityComputeNode, EntityGraphicsNode)
```

## Peripheral Dependencies

### Core Dependencies (vulkan/core/)
- **VulkanContext** - Device, queues, function loader, physical device properties
- **vulkan_raii** - RAII wrappers for automatic Vulkan resource cleanup  
- **VulkanManagerBase** - Base class providing common Vulkan manager functionality

### Integration Points (vulkan/nodes/)
- **EntityComputeNode** - Uses ComputePipelineManager for entity movement calculations
- **EntityGraphicsNode** - Uses GraphicsPipelineManager for instanced entity rendering
- **FrameGraph** - Provides resource dependencies and command buffer coordination

### External Systems
- **GPU Entity Management (ecs/gpu/)** - Provides GPU entity data layout and synchronization
- **Render Services (vulkan/services/)** - High-level rendering coordination and culling

## Key Implementation Notes

### Caching Strategy
- **LRU eviction** with frame-based tracking (`lastUsedFrame`, `useCount`)
- **Hardware-aware sizing**: Compute (512), Graphics (1024), Layouts (256), Shaders (512)
- **Usage patterns**: Frequent pipelines get cache priority and optimized storage

### Hardware Optimization  
- **Workgroup sizing**: `ComputeDeviceInfo` provides hardware-specific optimal dispatch parameters
- **Bindless support**: Feature detection with graceful fallback for older hardware
- **Batch operations**: Multi-pipeline creation reduces driver overhead

### Resource Management
- **RAII throughout**: `vulkan_raii::Pipeline`, `vulkan_raii::ShaderModule`, etc.
- **Explicit cleanup order**: `cleanupBeforeContextDestruction()` prevents use-after-free
- **Memory barrier optimization**: Intelligent barrier insertion for compute-compute dependencies

### Performance Features
- **Async compilation**: Background shader compilation with `std::future` tracking
- **Hot reload**: Development-time shader reloading with callback notifications  
- **Preset systems**: Pre-configured pipeline states for common patterns
- **Statistics tracking**: Hit ratios, compilation times, frame-based metrics

### Error Handling
- Pipeline state validation before compilation
- SPIR-V bytecode validation for shader modules
- Hardware capability checks with feature fallbacks
- Graceful degradation for missing device features

## Integration Gotchas

1. **Initialization Order**: ShaderManager and DescriptorLayoutManager must initialize before pipeline managers
2. **Frame Synchronization**: Cache statistics and LRU tracking are frame-based; call `resetFrameStats()` per frame
3. **Hot Reload Thread Safety**: Shader reload callbacks execute on main thread only
4. **Bindless Requirements**: Check hardware support before creating bindless layouts
5. **Cache Invalidation**: Swapchain recreation requires `recreateAllPipelineCaches()` call

**Note**: If any referenced VulkanContext, shader files, or descriptor layout specifications change, this documentation must be updated to reflect the new interfaces and data flows.