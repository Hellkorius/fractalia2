# Vulkan Render Pipeline Technical Reference

## Architecture Overview

The rendering system uses **instanced drawing** to render thousands of entities efficiently. Each shape type (triangle, square) has separate vertex/index buffers but shares a common instance buffer containing transformation matrices.

## Core Components

### Entity Processing Flow
```
ECS Entities → VulkanRenderer::updateEntities() → Instance Buffer → GPU
```

**Files**: `main.cpp:167-188`, `vulkan_renderer.cpp:380-382`

### Buffer Architecture
- **Vertex Buffers**: Per-shape geometry (`triangleVertexBuffer`, `squareVertexBuffer`)
- **Index Buffers**: Per-shape indices (`triangleIndexBuffer`, `squareIndexBuffer`) 
- **Instance Buffer**: Shared `glm::mat4` transformation matrices
- **Uniform Buffer**: View/projection matrices (128 bytes)

**Files**: `vulkan_resources.h:71-82`, `vulkan_resources.cpp:147-243`

### Vertex Input Layout
```glsl
// Binding 0: Per-vertex data (stride: 24 bytes)
layout(location = 0) in vec3 inPosition;    // offset: 0
layout(location = 1) in vec3 inColor;       // offset: 12

// Binding 1: Per-instance data (stride: 64 bytes) 
layout(location = 2) in vec4 instanceMatrix0; // offset: 0
layout(location = 3) in vec4 instanceMatrix1; // offset: 16
layout(location = 4) in vec4 instanceMatrix2; // offset: 32
layout(location = 5) in vec4 instanceMatrix3; // offset: 48
```

**Files**: `vulkan_pipeline.cpp:186-234`, `shaders/vertex.vert:8-13`

## Rendering Process

### 1. Entity Collection (`main.cpp:167-182`)
```cpp
world.query<Position, Shape, Color>().each([&](/* ... */) {
    renderEntities.emplace_back(pos, shapeType, color);
});
```

### 2. Instance Buffer Update (`vulkan_renderer.cpp:347-384`)
```cpp
// Process triangles first (instanceIndex 0, 1, 2...)
for (entity : renderEntities) {
    if (triangle) matrices[instanceIndex++] = transform;
}
// Process squares next (continuing instanceIndex...)
for (entity : renderEntities) {
    if (square) matrices[instanceIndex++] = transform;
}
```

### 3. Draw Call Execution (`vulkan_renderer.cpp:275-320`)
```cpp
// Triangles
VkBuffer triangleBuffers[] = {triangleVertexBuffer, instanceBuffer};
VkDeviceSize offsets[] = {0, 0}; // Triangle instances start at offset 0
vkCmdBindVertexBuffers(commandBuffer, 0, 2, triangleBuffers, offsets);
vkCmdDrawIndexed(commandBuffer, 3, triangleCount, 0, 0, 0);

// Squares  
VkBuffer squareBuffers[] = {squareVertexBuffer, instanceBuffer};
VkDeviceSize offsets[] = {0, triangleCount * sizeof(glm::mat4)}; // Offset past triangles
vkCmdBindVertexBuffers(commandBuffer, 0, 2, squareBuffers, offsets);
vkCmdDrawIndexed(commandBuffer, 6, squareCount, 0, 0, 0);
```

## Critical Implementation Details

### Instance Offset Handling
**INCORRECT** (causes double-offset bug):
```cpp
VkDeviceSize offsets[] = {0, instanceOffset * sizeof(glm::mat4)};
vkCmdDrawIndexed(/* ... */, instanceOffset); // Wrong: offset applied twice
```

**CORRECT**:
```cpp
VkDeviceSize offsets[] = {0, instanceOffset * sizeof(glm::mat4)};
vkCmdDrawIndexed(/* ... */, 0); // firstInstance = 0, offset handled by buffer binding
```

### Uniform Buffer Sizing
The UBO contains two `mat4` matrices:
```cpp
struct UniformBufferObject {
    glm::mat4 view;  // 64 bytes
    glm::mat4 proj;  // 64 bytes
};
// Total: 128 bytes, NOT 64!
```

**File**: `vulkan_resources.cpp:104`, `vulkan_renderer.cpp:332-335`

### Vertex Data Structure
```cpp
struct Vertex {
    glm::vec3 pos;    // 12 bytes
    glm::vec3 color;  // 12 bytes
    // Total: 24 bytes stride
};
```

**File**: `PolygonFactory.h:6-9`

## Pipeline State

### Rasterization
- **Cull Mode**: `VK_CULL_MODE_NONE` (disabled for development)
- **Front Face**: `VK_FRONT_FACE_CLOCKWISE`
- **Polygon Mode**: `VK_POLYGON_MODE_FILL`

**File**: `vulkan_pipeline.cpp:284-292`

### Multisampling
- **MSAA**: 4x (`VK_SAMPLE_COUNT_4_BIT`)
- **Sample Shading**: Disabled

**File**: `vulkan_pipeline.cpp:294-297`

## Debugging Tips

### Common Issues
1. **Nothing renders**: Check uniform buffer size (128 vs 64 bytes)
2. **Only one shape type renders**: Check instance offset calculation
3. **Shapes in wrong positions**: Verify entity→matrix ordering matches draw call ordering
4. **Faces missing**: Disable face culling during development

### Debug Output Locations
- Entity collection: `main.cpp:174-176`
- Instance matrices: `vulkan_renderer.cpp:385-390` 
- Draw calls: `vulkan_renderer.cpp:277-281`, `290-316`
- Buffer creation: `vulkan_resources.cpp:239-253`

## Performance Considerations

### Current Bottlenecks
1. **CPU-side entity processing**: O(n) loops in `updateInstanceBuffer()`
2. **Multiple draw calls**: One per shape type
3. **Uniform buffer updates**: Per-frame matrix uploads

### Scaling Strategies
1. **Compute shaders**: Move matrix calculations to GPU
2. **Indirect drawing**: Single draw call for all shapes
3. **GPU-driven rendering**: Eliminate CPU→GPU synchronization
4. **Instanced vertex pulling**: Remove vertex buffers, use SSBO indexing