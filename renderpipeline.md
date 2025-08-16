# Fractalia2 Render Pipeline

## Overview
Fractalia2 uses a **hybrid compute-graphics Vulkan pipeline** designed for high-performance rendering of hundreds of thousands of discrete 2D entities. The system employs modular compute shaders for movement calculation and instanced graphics rendering to achieve 60 FPS with massive entity counts.

## Pipeline Architecture

### Core Components
```
VulkanRenderer (Master Controller)
├── VulkanContext       // Instance, device, queues
├── VulkanSwapchain     // Swapchain, framebuffers, MSAA
├── VulkanPipeline      // Shaders, render pass, pipeline
├── VulkanResources     // Buffers, descriptors, uniforms  
├── VulkanSync          // Fences, semaphores, command pools
├── ResourceContext     // Buffer/memory management
└── GPUEntityManager    // Entity storage, CPU→GPU bridge
```

### Initialization Flow
1. **Context Setup**: Instance, device, queues, debug layers
2. **Swapchain Creation**: Surface, format selection, 4× MSAA setup
3. **Pipeline Creation**: Shader loading, render pass, pipeline state
4. **Framebuffer Creation**: MSAA color/depth attachments
5. **Resource Allocation**: Uniform buffers, vertex buffers, descriptor sets
6. **GPU Entity System**: Instance buffer setup, staging system

## Rendering Pipeline

### Frame Loop (`VulkanRenderer::drawFrame()`)
1. **Fence Wait**: Synchronize with previous frame completion
2. **Swapchain Acquire**: Get next framebuffer image
3. **Uniform Updates**: Camera matrices, time, entity count
4. **Compute Dispatch**: Execute modular movement calculation shaders
5. **Memory Barrier**: Ensure compute writes complete before graphics read
6. **Graphics Recording**: Record rendering commands with pre-computed positions
7. **Draw Submission**: Submit command buffer with semaphore sync
8. **Present**: Display frame with proper synchronization

### Compute Pass Recording
```cpp
// Select appropriate compute pipeline based on movement type
VkPipeline selectedPipeline = (movementType == 4) ? 
    randomComputePipeline : patternComputePipeline;

// Bind compute pipeline  
vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, selectedPipeline);

// Bind compute descriptor sets (entity buffer + position buffer)
vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                       computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);

// Push constants (time, delta time, entity count, frame)
vkCmdPushConstants(computeCommandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                  0, sizeof(ComputePushConstants), &computePushConstants);

// Dispatch compute shader (64 threads per workgroup)
uint32_t numWorkgroups = (entityCount + 63) / 64;
vkCmdDispatch(computeCommandBuffer, numWorkgroups, 1, 1);

// Memory barrier: compute writes → graphics reads
vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
```

### Graphics Pass Recording
```cpp
// Begin render pass with MSAA
vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

// Bind graphics pipeline
vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

// Bind graphics descriptors (UBO + position buffer)
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

// Bind vertex and instance buffers 
VkBuffer vertexBuffers[] = {vertexBuffer, instanceBuffer};
VkDeviceSize offsets[] = {0, 0};
vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

// Bind index buffer
vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

// Single indexed instanced draw call
vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);

vkCmdEndRenderPass(commandBuffer);
```

## Instanced Rendering System

### Vertex Input Layout
- **Binding 0** (Vertex Rate): `vec3 inPos` - Triangle geometry (location 0)
- **Binding 1** (Instance Rate): GPUEntity instance data (locations 2-9)
- **Position Buffer**: Pre-computed positions from compute shaders (binding 2)

### Hybrid CPU/GPU Data Flow
1. **CPU Creation**: Flecs ECS components → `EntityFactory`
2. **CPU→GPU Upload**: `GPUEntityManager::addEntitiesFromECS()`
3. **GPU Entity Storage**: Instance buffer with 128-byte `GPUEntity` structs
4. **Compute Processing**: Modular compute shaders calculate movement positions
5. **GPU Position Storage**: Position buffer with pre-computed vec4 positions
6. **Graphics Processing**: Vertex shader reads pre-computed positions for rendering
7. **Fragment Processing**: Dynamic color generation via HSV→RGB

### Modular Movement Computation
The system uses specialized compute shaders for movement calculation:
- **Pattern Shader** (`movement_pattern.comp`): Handles types 0-3
  - **Petal (0)**: Breathing radius with circular motion
  - **Orbit (1)**: Fixed-radius circular orbit  
  - **Wave (2)**: Sinusoidal X/Y wave patterns
  - **Triangle (3)**: Dynamic radius triangular orbit
- **Random Shader** (`movement_random.comp`): Handles type 4
  - **Random Walk (4)**: Wang hash-based pseudorandom step movement with drift correction

## Shader Pipeline

### Compute Shaders
#### Movement Pattern Compute (`movement_pattern.comp`)
```glsl
// Push constants: Temporal data
layout(push_constant) uniform ComputePushConstants {
    float time;
    float deltaTime;
    uint entityCount;
    uint frame;
} pc;

// Input: GPUEntity buffer (binding 0)
layout(std430, binding = 0) readonly buffer GPUEntityBuffer {
    vec4 entityData[];  // 128-byte GPUEntity structs
} entities;

// Output: Position buffer (binding 1)
layout(std430, binding = 1) writeonly buffer PositionBuffer {
    vec4 positions[];   // xyz position + w unused
} outPositions;

// Compute processing:
// 1. Extract movement parameters from entity data
// 2. Calculate pattern-based movement (types 0-3)
// 3. Write computed position to position buffer
```

#### Random Walk Compute (`movement_random.comp`)
```glsl
// Same interface as pattern shader, but implements:
// 1. Wang hash pseudorandom number generation
// 2. Accumulated random step movement
// 3. Boundary constraints and center drift correction
```

### Graphics Shaders
#### Vertex Shader (`vertex.vert`)
```glsl
// Uniforms: Camera matrices
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
} ubo;

// Position buffer: Pre-computed positions from compute shaders
layout(std430, binding = 2) readonly buffer PositionBuffer {
    vec4 positions[];
} positionBuffer;

// Instance inputs: GPUEntity data for color/transform
layout(location = 2) in mat4 modelMatrix;      // locations 2-5
layout(location = 6) in vec4 color;            // location 6
layout(location = 7) in vec4 movementParams0;  // location 7
layout(location = 8) in vec4 movementParams1;  // location 8
layout(location = 9) in vec4 runtimeState;     // location 9

// Per-vertex processing:
// 1. Read pre-computed world position from position buffer
// 2. Generate dynamic color via HSV
// 3. Apply rotation transform
// 4. Project to screen space
```

#### Fragment Shader (`fragment.frag`)
Simple interpolated color pass-through for GPU-generated colors.

## Resource Management

### Buffer Strategy
- **Shared Vertex Buffer**: Single triangle geometry for all entities
- **GPUEntity Instance Buffer**: Per-entity movement parameters and state (128 bytes each)
- **Position Buffer**: Pre-computed positions from compute shaders (16 bytes each)
- **Uniform Buffers**: Camera matrices (per frame in flight)
- **Staging System**: Efficient CPU→GPU transfers

### Memory Layout
```cpp
struct GPUEntity {           // 128 bytes total
    glm::mat4 modelMatrix;   // 64 bytes - reserved for future use
    glm::vec4 color;         // 16 bytes - reserved for future use  
    glm::vec4 movementParams0; // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1; // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;    // 16 bytes - totalTime, initialized, stateTimer, entityState
};
```

### Synchronization
- **Double Buffering**: 2 frames in flight for smooth rendering
- **Fence Synchronization**: CPU waits for GPU completion
- **Compute→Graphics Barrier**: Memory barrier ensures compute writes complete before graphics reads
- **Semaphore Chains**: Image acquire → compute → render → present
- **Pipeline Dependencies**: Proper GPU→GPU synchronization between compute and graphics stages

## Performance Characteristics

### Rendering Efficiency
- **Single Compute Dispatch**: Modular compute shader selection based on movement type
- **Single Draw Call**: All entities rendered in one instanced draw
- **GPU Position Calculation**: No CPU→GPU position uploads per frame
- **Pre-computed Positions**: Vertex shader reads from position buffer instead of calculating
- **Minimal State Changes**: Shared pipeline, descriptors, geometry
- **MSAA**: 4× anti-aliasing with resolve to swapchain

### Scalability
- **Instance Limit**: 131,072 entities (128k max) in single draw call
- **Current Capacity**: ~90k entities rendering smoothly
- **Memory Efficient**: 144 bytes per entity total (128 bytes entity data + 16 bytes position)
- **Total Memory**: ~18.5 MB for 128k entities (entity buffer + position buffer)
- **Compute Efficiency**: 64 threads per workgroup, optimal GPU utilization
- **GPU Bottleneck**: Fragment fill rate becomes limiting factor
- **CPU Overhead**: Minimal per-frame CPU work after setup

### Debug Features
- Entity count validation and overflow protection
- GPU/CPU entity count comparison
- Memory usage tracking via `MemoryManager`
- Performance profiling via `Profiler`