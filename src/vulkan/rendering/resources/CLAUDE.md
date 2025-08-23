# vulkan/rendering/resources

(Manages frame graph resource lifecycle including Vulkan buffers/images with RAII wrappers and memory pressure-aware allocation strategies.)

## Files

### resource_manager.h
**Inputs:** VulkanContext, GPUMemoryMonitor, resource specifications (buffer size/usage, image format/extent).
**Outputs:** FrameGraphTypes::ResourceId handles for created/imported resources, VkBuffer/VkImage/VkImageView handles for access.
**Purpose:** Defines ResourceManager class with RAII-wrapped FrameGraphBuffer/FrameGraphImage structs, allocation telemetry tracking, and resource criticality classification for memory management.

### resource_manager.cpp  
**Inputs:** Resource creation parameters, external Vulkan objects for import, resource IDs for access/cleanup.
**Outputs:** Created Vulkan resources with allocated memory, resource eviction operations, allocation performance telemetry.
**Purpose:** Implements multi-strategy allocation with criticality-based retry logic, resource lifecycle management with cleanup tracking, and memory pressure response through eviction of non-critical resources.