# /src/vulkan/rendering/execution

(Manages Vulkan memory barriers and synchronization for optimal async compute-graphics pipeline execution)

## Files

### barrier_manager.h
**Inputs:** Frame graph execution order, frame graph nodes with resource dependencies, VulkanContext for API access.  
**Outputs:** Optimized barrier batches grouped by target nodes, resource access tracking for O(n) barrier analysis.  
**Function:** Defines barrier management interface with per-node tracking structures and resource accessor injection.

### barrier_manager.cpp
**Inputs:** Frame graph node inputs/outputs, resource write tracking, execution order sequence.  
**Outputs:** VkBufferMemoryBarrier and VkImageMemoryBarrier commands inserted into command buffers at optimal points.  
**Function:** Analyzes compute-to-graphics transitions, creates batched barriers to prevent hazards while maximizing async execution.