# CLAUDE.md

## src/vulkan/pipelines/

(Unified pipeline management system with direct VulkanContext access, eliminating VulkanManagerBase inheritance for improved performance. Features caching, hot reload, and optimization for both graphics and compute pipelines using VulkanFunctionLoader pattern.)

### Compute Pipeline Components

**compute_device_info.h/cpp**  
Inputs: VulkanContext, device properties/features queries. Outputs: Optimal workgroup sizes, device capability data, compute-specific optimization parameters for pipeline creation.

**compute_dispatcher.h/cpp**  
Inputs: Command buffers, compute dispatch parameters, buffer/image barriers. Outputs: Optimized compute dispatches, barrier insertion, dispatch statistics and performance tracking.

**compute_pipeline_cache.h/cpp**  
Inputs: ComputePipelineState specifications, compilation callbacks. Outputs: Cached VkPipeline objects, hit/miss statistics, LRU eviction management for compute pipelines.

**compute_pipeline_factory.h/cpp**  
Inputs: ComputePipelineState, ShaderManager, RAII pipeline cache. Outputs: CachedComputePipeline instances, VkPipelineLayout objects, shader specialization and validation.

**compute_pipeline_manager.h/cpp**  
Inputs: Shader/descriptor layout managers, pipeline states, dispatch requests. Outputs: Unified compute pipeline operations, batch compilation, async loading, profiling data and preset configurations.

**compute_pipeline_types.h/cpp**  
Inputs: Pipeline specifications, workgroup parameters, specialization constants. Outputs: ComputePipelineState structs, dispatch optimization data, cached pipeline metadata with performance metrics.

### Graphics Pipeline Components

**graphics_pipeline_cache.h/cpp**  
Inputs: GraphicsPipelineState objects, pipeline creation callbacks. Outputs: Cached graphics VkPipeline objects, usage statistics, memory-efficient caching with eviction policies.

**graphics_pipeline_factory.h/cpp**  
Inputs: GraphicsPipelineState, render passes, shader modules. Outputs: Complete graphics pipeline objects, pipeline layout creation, state validation and compilation timing.

**graphics_pipeline_layout_builder.h/cpp**  
Inputs: Descriptor set layouts, push constant ranges, graphics requirements. Outputs: VkPipelineLayout objects, layout compatibility validation, builder pattern for complex layouts.

**graphics_pipeline_manager.h/cpp**  
Inputs: Shader/layout managers, graphics states, render pass specifications. Outputs: Graphics pipeline operations, MSAA/wireframe presets, hot reload support, integrated cache management.

**graphics_pipeline_state_hash.h/cpp**  
Inputs: GraphicsPipelineState components, vertex attributes, blend states. Outputs: Hash values for pipeline caching, state comparison utilities, optimized hash computation.

**graphics_render_pass_manager.h/cpp**  
Inputs: Color/depth formats, sample counts, MSAA requirements. Outputs: VkRenderPass objects, render pass caching, format compatibility validation and subpass management.

### Descriptor and Layout Management

**descriptor_layout_manager.h/cpp**  
Inputs: DescriptorLayoutSpec, binding configurations, device capabilities. Outputs: VkDescriptorSetLayout objects, descriptor pool sizing, bindless layout support, usage analytics.

### Shader Management

**shader_manager.h/cpp**  
Inputs: SPIR-V files, GLSL source, compilation parameters, hot reload configuration. Outputs: VkShaderModule objects, shader reflection data, compilation statistics, automatic recompilation on file changes.

### System Coordination

**pipeline_system_manager.h/cpp**  
Inputs: VulkanContext, initialization parameters. Outputs: Unified access to all pipeline managers, integrated statistics, coordinated cache optimization and system-wide pipeline operations.

### Utilities

**hash_utils.h**  
Inputs: Various data types, containers, pipeline state components. Outputs: Hash combination utilities, consistent hash generation, cache key computation with collision avoidance.

**pipeline_utils.h/cpp**  
Inputs: Vulkan objects, pipeline specifications, creation parameters. Outputs: Common pipeline creation utilities, render pass helpers, barrier generation, debug naming functions.