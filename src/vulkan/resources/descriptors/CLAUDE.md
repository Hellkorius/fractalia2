# /src/vulkan/resources/descriptors

## Folder Structure
```
descriptors/                    (Provides base classes and utilities for Vulkan descriptor set management and updates)
├── descriptor_set_manager_base.h
├── descriptor_set_manager_base.cpp
├── descriptor_update_helper.h
└── descriptor_update_helper.cpp
```

## Files

### descriptor_set_manager_base.h
**Inputs:** None (header defining base class interface)  
**Outputs:** Abstract base class for descriptor set managers with template method pattern, validation helpers, and shared pool management infrastructure.

### descriptor_set_manager_base.cpp
**Inputs:** VulkanContext reference, specialized manager implementations via virtual methods  
**Outputs:** Initialized descriptor pool manager, coordinated lifecycle management for specialized descriptor managers, context and handle validation.

### descriptor_update_helper.h
**Inputs:** None (header defining static utility functions)  
**Outputs:** Template functions and data structures for descriptor set update operations with buffer binding specifications.

### descriptor_update_helper.cpp
**Inputs:** VulkanContext, VkDescriptorSet handles, BufferBinding vectors with buffer handles and descriptor types  
**Outputs:** Updated descriptor sets via vkUpdateDescriptorSets, validation of buffer bindings and descriptor handles.