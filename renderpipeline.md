# Fractalia2 Render Pipeline

## Overview
Fractalia2 uses a **modular Vulkan instanced rendering pipeline** designed for high-performance rendering of hundreds of thousands of discrete 2D entities. The pipeline uses GPU-driven transform calculations and dynamic color generation to achieve 60 FPS with massive entity counts.

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
3. **Command Recording**: Record rendering commands
4. **Uniform Updates**: Camera matrices, time, entity count
5. **Entity Processing**: GPU entity buffer updates
6. **Draw Submission**: Submit command buffer with semaphore sync
7. **Present**: Display frame with proper synchronization

### Command Buffer Recording
```cpp
// Begin render pass with MSAA
vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

// Bind graphics pipeline
vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

// Bind uniform descriptors (camera matrices)
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

// Push constants (time, delta time, entity count)
vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 
                  0, sizeof(PushConstants), &pushConstants);

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
- **Binding 1** (Instance Rate): `vec4 ampFreqPhaseOff` - Movement parameters (location 7)
- **Binding 1** (Instance Rate): `vec4 centerType` - Center position + movement type (location 8)

### GPU Entity Data Flow
1. **CPU Creation**: Flecs ECS components → `EntityFactory`
2. **CPU→GPU Upload**: `GPUEntityManager::addEntitiesFromECS()`
3. **GPU Storage**: Instance buffer with 128-byte `GPUEntity` structs
4. **Vertex Processing**: Shader calculates transforms from movement parameters
5. **Fragment Processing**: Dynamic color generation via HSV→RGB

### Movement Patterns (GPU Calculated)
The vertex shader implements four movement patterns:
- **Petal (0)**: Breathing radius with circular motion
- **Orbit (1)**: Fixed-radius circular orbit  
- **Wave (2)**: Sinusoidal X/Y wave patterns
- **Triangle (3)**: Dynamic radius triangular orbit

## Shader Pipeline

### Vertex Shader (`vertex.vert`)
```glsl
// Uniforms: Camera matrices
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
} ubo;

// Push constants: Frame data
layout(push_constant) uniform PC {
    float time;
    float dt;
    uint  count;
} pc;

// Instance inputs: Movement parameters
layout(location = 7) in vec4 ampFreqPhaseOff;  // amplitude, frequency, phase, timeOffset
layout(location = 8) in vec4 centerType;       // center.xyz, movementType

// Per-vertex processing:
// 1. Calculate world position from movement pattern
// 2. Generate dynamic color via HSV
// 3. Apply rotation transform
// 4. Project to screen space
```

### Fragment Shader (`fragment.frag`)
Simple interpolated color pass-through for GPU-generated colors.

## Resource Management

### Buffer Strategy
- **Shared Vertex Buffer**: Single triangle geometry for all entities
- **Instance Buffer**: Per-entity movement parameters and state
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
- **Semaphore Chains**: Image acquire → render → present
- **Memory Barriers**: Proper GPU→GPU synchronization

## Performance Characteristics

### Rendering Efficiency
- **Single Draw Call**: All entities rendered in one instanced draw
- **GPU Transform Calculation**: No CPU→GPU matrix uploads per frame
- **Minimal State Changes**: Shared pipeline, descriptors, geometry
- **MSAA**: 4× anti-aliasing with resolve to swapchain

### Scalability
- **Instance Limit**: 131,072 entities (128k max) in single draw call
- **Current Capacity**: ~90k entities rendering smoothly
- **Memory Efficient**: 128 bytes per entity (16 MB for 128k entities)
- **GPU Bottleneck**: Fragment fill rate becomes limiting factor
- **CPU Overhead**: Minimal per-frame CPU work after setup

### Debug Features
- Entity count validation and overflow protection
- GPU/CPU entity count comparison
- Memory usage tracking via `MemoryManager`
- Performance profiling via `Profiler`