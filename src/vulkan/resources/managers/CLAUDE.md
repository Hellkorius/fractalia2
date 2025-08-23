# managers/

## Folder Overview
**(High-level resource management coordination for Vulkan graphics pipeline components)**

## File Hierarchy

### descriptor_pool_manager.h
**Inputs:** VulkanContext reference, DescriptorPoolConfig specifications (maxSets, buffer counts, pool flags).  
**Outputs:** Creates RAII-wrapped VkDescriptorPool instances with configurable resource limits.  
**Function:** Provides centralized descriptor pool allocation with bindless-ready configuration options.

### descriptor_pool_manager.cpp
**Inputs:** Pool configuration parameters and VulkanContext for device access.  
**Outputs:** Configures VkDescriptorPoolSize arrays and creates pools via vulkan_raii factory.  
**Function:** Implements pool creation with proper size distribution for uniform/storage buffers and images.

### graphics_resource_manager.h
**Inputs:** VulkanContext, BufferFactory, descriptor set layouts, buffer handles from other subsystems.  
**Outputs:** Manages complete graphics pipeline resources including uniform buffers, vertex/index buffers, descriptor pools and sets.  
**Function:** Consolidated facade for graphics resource lifecycle with DRY descriptor updates and swapchain recreation handling.

### graphics_resource_manager.cpp
**Inputs:** Triangle geometry from PolygonFactory, staging buffers, uniform buffer data (MVP matrices).  
**Outputs:** Creates device-local vertex/index buffers, allocates descriptor sets, updates buffer bindings via DescriptorUpdateHelper.  
**Function:** Implements full graphics resource creation pipeline with memory optimization and automatic descriptor recreation during swapchain rebuilds.