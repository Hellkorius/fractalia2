# /home/nar/projects/fractalia2/src/vulkan/core

*Core Vulkan infrastructure providing centralized device context, function loading, RAII resource management, and foundational utilities for the entire Vulkan subsystem.*

```
src/vulkan/core/
├── QUEUE_SYSTEM_OVERVIEW.md          # Documentation for queue system architecture
├── queue_manager.cpp                 # Queue management implementation
├── queue_manager.h                   # Queue and command buffer management system
├── vulkan_constants.h                # Global constants and configuration values
├── vulkan_context.cpp                # Vulkan context implementation
├── vulkan_context.h                  # Central Vulkan device and queue context
├── vulkan_function_loader.cpp        # Function loader implementation
├── vulkan_function_loader.h          # Centralized Vulkan function pointer management
├── vulkan_raii.cpp                   # RAII wrapper implementations
├── vulkan_raii.h                     # RAII wrappers for Vulkan objects
├── vulkan_swapchain.cpp              # Swapchain management implementation
├── vulkan_swapchain.h                # Swapchain creation and recreation
├── vulkan_sync.cpp                   # Synchronization object implementation
├── vulkan_sync.h                     # Vulkan synchronization primitives
├── vulkan_utils.cpp                  # Utility function implementations
└── vulkan_utils.h                    # Common Vulkan utility functions
```

## Files

**vulkan_constants.h**
- **Inputs**: None (header-only constants)
- **Outputs**: Engine-wide configuration constants for frame counts, timeouts, cache sizes, memory limits, and compute workgroup parameters. Provides single source of truth for all Vulkan subsystem sizing.

**vulkan_context.h**
- **Inputs**: SDL window handle, validation layer requirements
- **Outputs**: Complete Vulkan context with instance, device, physical device, queue handles, and queue family indices. Exposes centralized VulkanFunctionLoader access and queue capability queries.

**vulkan_context.cpp**
- **Inputs**: SDL window handle, required extensions, validation layers
- **Outputs**: Initialized Vulkan instance/device/surface, loaded function pointers, configured debug messenger, and discovered queue families. Handles device suitability evaluation and queue family optimization with fallbacks.

**vulkan_function_loader.h**
- **Inputs**: SDL window handle, Vulkan instance/device handles
- **Outputs**: Centralized function pointer management with organized loading phases (pre-instance, post-instance, post-device). Provides single source of truth for all Vulkan API functions across the engine.

**vulkan_function_loader.cpp**
- **Inputs**: SDL Vulkan proc addr, instance/device handles
- **Outputs**: Loaded function pointers organized by category (core, physical device, surface, memory, buffers, images, pipelines, descriptors, synchronization, commands). Uses macros to eliminate repetitive loading patterns.


**vulkan_raii.h**
- **Inputs**: Raw Vulkan handles and VulkanContext for cleanup
- **Outputs**: Template-based RAII wrappers with move semantics, automatic cleanup, and factory functions. Provides type-safe resource management for all Vulkan objects with explicit cleanup ordering support.

**vulkan_raii.cpp**
- **Inputs**: Vulkan handles, VulkanContext for function loader access
- **Outputs**: Implemented deleter functions using generic template pattern and specialized memory management for VkDeviceMemory. Includes direct creation factory functions for common pipeline operations.

**vulkan_swapchain.h**
- **Inputs**: VulkanContext, SDL window, render pass for framebuffer creation
- **Outputs**: Swapchain management with images, image views, MSAA color resources, and framebuffers. Provides extent/format queries and recreation support for window resize events.

**vulkan_swapchain.cpp**
- **Inputs**: Window surface capabilities, format preferences, present mode requirements
- **Outputs**: Created swapchain with optimal configuration, MSAA support, framebuffers, and recreation logic. Handles cleanup ordering for resize scenarios and debugging output for troubleshooting.

**vulkan_sync.h**
- **Inputs**: VulkanContext for synchronization object creation
- **Outputs**: Frame-indexed semaphores and fences for graphics/compute/presentation synchronization. Provides both individual access and bulk vector retrieval for compatibility with existing rendering code.

**vulkan_sync.cpp**
- **Inputs**: VulkanContext, MAX_FRAMES_IN_FLIGHT configuration
- **Outputs**: Created synchronization objects with proper initialization (fences start signaled) and RAII cleanup. Handles bounds checking and error reporting for sync object access.

**vulkan_utils.h**
- **Inputs**: Various Vulkan objects, file paths, memory requirements
- **Outputs**: Consolidated utility functions for memory allocation, buffer/image creation, shader loading, command buffer utilities, descriptor updates, synchronization helpers, and error handling. Eliminates code duplication across modules.

**vulkan_utils.cpp**
- **Inputs**: Physical device properties, memory type filters, buffer/image specifications, shader code, command requirements
- **Outputs**: Created Vulkan resources with proper memory binding, single-time command execution, image transitions, buffer copies, descriptor set updates, and comprehensive error logging with VkResult interpretation.

**queue_manager.h**
- **Inputs**: VulkanContext for queue/command pool initialization
- **Outputs**: Centralized queue access with automatic fallbacks, specialized command pools, frame-based command buffers, one-time transfer commands with completion tracking, and utilization telemetry. Abstracts queue family complexity.

**queue_manager.cpp**
- **Inputs**: VulkanContext, CommandPoolType specifications, frame indices
- **Outputs**: Initialized command pools with appropriate flags, allocated command buffers for all frames, transfer command allocation/deallocation with fence tracking, and detailed telemetry logging. Provides queue capability reporting and command buffer lifecycle management.