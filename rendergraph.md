# Render Graph Architecture

## Status: ✅ OPERATIONAL - 80,000 entities at 60+ FPS

## Design
**FrameGraph**: Resource tracking, barrier insertion, node execution
**VulkanRenderer**: Queue submission, synchronization, swapchain management
**Nodes**: Declare dependencies, frame graph handles barriers automatically

## Implemented Nodes ✅ WORKING

### EntityComputeNode
- Dispatches compute shader for 80,000 entities (2500 workgroups)
- Resources: ReadWrite entity/current/target buffers, Write position buffer
- Automatic barriers to graphics stage

### EntityGraphicsNode  
- Renders 80,000 entities with instanced drawing
- Frame-in-flight safe uniform buffer updates
- Integrated camera matrix management

### SwapchainPresentNode
- Declares dependency on color target for proper synchronization

## Features ✅
- **Automatic barriers**: SHADER_WRITE → VERTEX_ATTRIBUTE_READ for entity buffer
- **Resource tracking**: 4 buffers (entity, position, current, target) + swapchain image
- **Frame safety**: Frame-in-flight index validation (0-1, not unbounded counter)
- **Selective execution**: Only begin/end command buffers that are actually used

## Remaining Work ❌
- **Legacy cleanup**: Remove unused methods from VulkanRenderer
- **Additional nodes**: Shadow mapping, post-processing, etc.
- **Multi-queue**: Async compute on dedicated queue families

## Execution Flow
1. **VulkanRenderer**: waitForFences() → acquireImage() → uploadEntities()
2. **FrameGraph**: reset() → importResources() → addNodes() → compile() → execute()
3. **VulkanRenderer**: submitCompute() → submitGraphics() → present()

## Files
- `src/vulkan/frame_graph.h/.cpp` - Core coordinator
- `src/vulkan/nodes/entity_*.h/.cpp` - Node implementations  
- `src/vulkan_renderer.h/.cpp` - Integration point