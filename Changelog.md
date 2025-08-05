## [Initial] - 2025-08-04

- **Initial Project Structure**: Set up the basic file hierarchy, including directories for source code, shaders, and external libraries.
- **SDL3 Integration**: Integrated SDL3 for window management, input handling, and Vulkan surface creation.
- **Vulkan Setup**: Implemented the initial Vulkan setup, including instance creation, surface creation, and basic rendering pipeline.
- **Flecs ECS**: Integrated Flecs ECS for managing entities and components. Defined initial components (`Position`, `Velocity`, `Color`) and a simple movement system.
- **GLM Mathematics Library**: Included GLM for 3D transformations and vector/matrix operations.
- **MinGW Cross-Compilation**: Set up MinGW for cross-compiling Windows executables on Linux.
- **Build Scripts**: Created build scripts (`build.sh`, `build-fast.sh`, `compile-shaders.sh`) for compiling the project and managing DLLs.



### Added
- **Vulkan Instanced Rendering Pipeline**: Switched to an instanced rendering pipeline to support rendering tens of thousands of entities efficiently. Each entity now uses a `glm::mat4` transform stored in an instance buffer, allowing for dynamic transformations while minimizing draw calls.
- **Dynamic Descriptor Sets**: Expanded the descriptor pool to support one descriptor set per entity batch (up to 1024 sets). This allows each batch to bind its own transform data without thrashing the frame-level descriptor sets.
- **Staging Buffer Ring**: Implemented a ring of host-coherent staging buffers (4 × 1 MB) to facilitate efficient data transfer from the CPU to the GPU. Flecs systems can write transforms into the staging ring each frame, which are then copied to the instance buffer with a single `memcpy`.
- **Global Vertex and Index Buffers**: Created a single large device-local vertex and index buffer pair, fed by a host-visible staging buffer. This setup allows for efficient data management and reduces the overhead of buffer creation and destruction.

### Changed
- **Pipeline Configuration**: Updated the graphics pipeline to accept vertex and instance-rate inputs. The vertex shader now processes per-instance transforms, enabling dynamic rendering of entities.
- **Resource Management**: Refactored resource management to use a single global vertex/index buffer and a dynamic descriptor pool. This change improves performance and scalability by reducing the number of Vulkan API calls and resource allocations.

### Fixed
- **Command Pool Access**: Fixed an issue where `VulkanResources::copyBuffer` tried to access a non-existent `getCommandPool` method in `VulkanContext`. The method was added to provide access to the command pool.
- **Vertex Attribute Descriptions**: Corrected the scope of `getVertexBindingDescriptions` and `getVertexAttributeDescriptions` functions. They are now properly defined at the class scope, making them accessible to the `createGraphicsPipeline` method.