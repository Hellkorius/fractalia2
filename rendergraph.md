# Render Graph Implementation Plan

## Completed (Phase 1)
- ✅ **Core Framework**: `FrameGraph` class with resource/node abstractions
- ✅ **Resource Management**: External import + automatic Vulkan resource creation
- ✅ **Node System**: `FrameGraphNode` base class with dependency tracking
- ✅ **Basic Nodes**: Placeholder `ComputeNode` and `GraphicsNode` implementations

## Phase 2: Concrete Implementation ✅ COMPLETE
### 2.1 Current Pipeline Nodes ✅
- **EntityComputeNode**: ✅ Complete compute shader dispatch with memory barriers
- **EntityGraphicsNode**: ✅ Complete graphics pipeline with render pass management
- **SwapchainPresentNode**: ✅ Complete presentation dependency tracking

### 2.2 Integration ✅
- ✅ `VulkanRenderer::drawFrameWithFrameGraph()` implemented
- ✅ Import existing buffers: entity buffer, position buffer
- ✅ Import swapchain images as external resources per-frame
- ✅ Maintain current compute→graphics→present execution order
- ✅ Complete feature parity with legacy `drawFrame()`

### 2.3 Advanced Features (Future)
- Automatic synchronization barrier insertion
- Multi-frame resource overlap
- Dynamic graph compilation based on entity count
- GPU-driven culling passes
- Multiple render targets for post-processing

## Implemented File Hierarchy ✅
```
src/vulkan/
├── frame_graph.h/.cpp               # ✅ Core framework 
├── nodes/
│   ├── entity_compute_node.h/.cpp   # ✅ Compute movement calculation
│   ├── entity_graphics_node.h/.cpp  # ✅ Graphics rendering  
│   └── swapchain_present_node.h/.cpp # ✅ Presentation
└── vulkan_renderer.h/.cpp           # ✅ Frame graph integration
```

## Migration Strategy ✅ COMPLETE  
1. ✅ **Minimal Disruption**: Kept existing `GPUEntityManager`, `VulkanPipeline`, `ResourceContext` unchanged
2. ✅ **Parallel Implementation**: Created `drawFrameWithFrameGraph()` alongside legacy `drawFrame()`
3. ✅ **Validation**: Maintained identical visual output during transition
4. ✅ **Performance**: No performance regression vs. current direct Vulkan approach

## Next Steps (Phase 3: Cutover)
1. **Switch main loop**: Replace `drawFrame()` call with `drawFrameWithFrameGraph()`
2. **Remove legacy code**: Clean up monolithic `drawFrame()` implementation  
3. **Optimize**: Fine-tune frame graph execution and resource management

## Key Benefits
- **AAA Architecture**: Industry-standard render graph pattern
- **Resource Safety**: Automatic lifetime management prevents use-after-free
- **Extensibility**: Easy to add new passes (shadows, post-processing, etc.)
- **Debug/Profiling**: Clear pass boundaries for GPU profiler integration
- **Multi-threading**: Foundation for parallel command buffer recording