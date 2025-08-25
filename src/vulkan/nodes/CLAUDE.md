# Vulkan Nodes

## Directory Overview
Frame graph node implementations with MVP transformation system and model matrix-based physics updates.

### MVP Transformation Architecture
- **Model Matrix Centralization**: Physics updates model matrices directly, eliminating position buffer ping-pong
- **Simplified Resource Dependencies**: Reduced from 4 position buffers to single model matrix buffer
- **Clean MVP Pipeline**: Object → World (model) → View → Projection transformation in vertex shader
- **Unified Node Architecture**: BaseComputeNode template eliminates 300+ lines of compute node duplication

### Files

**base_compute_node.h**
- **Template Architecture**: Eliminates duplication between EntityComputeNode and PhysicsComputeNode
- **Inputs**: Entity buffer, model matrix buffer (single buffer replaces position ping-pong)
- **Function**: Provides unified compute execution with chunked dispatch, pipeline management, and resource binding

**entity_compute_node.h**
- **Inputs**: Entity buffer, position/model matrix buffers (legacy ping-pong structure maintained)
- **Outputs**: Updated entity movement parameters, velocity calculations
- **Function**: Random walk movement computation using BaseComputeNode template methods

**entity_compute_node.cpp**
- **Execution**: Delegates to BaseComputeNode::executeComputeNode() template method
- **Pipeline**: Uses EntityMovementComputeState with movement-specific parameters
- **Function**: Calculates entity velocity and movement patterns with adaptive workload management

**entity_graphics_node.h**
- **MVP Integration**: Reads model matrices from physics for proper 3D transformation
- **Inputs**: Entity buffer (attributes), model matrix buffer (transform data)
- **Outputs**: Rendered swapchain images with depth testing and MSAA resolve
- **Function**: 3D instanced rendering with camera matrix updates and uniform buffer optimization

**entity_graphics_node.cpp**
- **MVP Transformation**: Vertex shader reads model matrices and applies View/Projection from UBO
- **Resource Access**: ReadOnly access to entity and model matrix buffers
- **Rendering**: Dynamic rendering with depth attachment, MSAA (2x), and frustum culling
- **Function**: Executes 3D graphics pipeline with cached uniform buffers and dirty tracking

**physics_compute_node.h**
- **Model Matrix Output**: Constructor takes modelMatrixBuffer instead of separate position buffers
- **Simplified Dependencies**: ReadWrite access to entity buffer and model matrix buffer only
- **Function**: Spatial hash collision detection writing directly to model matrix translation components

**physics_compute_node.cpp**
- **Resource Simplification**: Uses single modelMatrixBuffer for all position operations
- **Constructor**: BaseComputeNode(entityBuffer, modelMatrixBuffer, modelMatrixBuffer, modelMatrixBuffer)
- **Dependencies**: ReadWrite access to model matrix buffer eliminates position buffer ping-pong
- **Execution**: Physics calculations write 3D positions to model matrix column 3 (translation)
- **Function**: 3D spatial hash (64×64×8 grid, 32,768 cells) with optimized cache-line access patterns

**swapchain_present_node.h**
- **Inputs**: Dynamic swapchain image resource ID resolved per-frame
- **Dependencies**: ColorAttachment write access to current swapchain image
- **Function**: Frame graph presentation dependency declaration without direct command operations

**swapchain_present_node.cpp**
- **Validation**: Ensures swapchain image bounds and presentation readiness
- **Dependencies**: Uses currentSwapchainImageId for dynamic per-frame resource binding
- **Function**: Validates presentation state for queue-level presentation coordination

## MVP Transformation Pipeline

### Data Flow
1. **Movement Compute**: EntityComputeNode updates velocity/movement parameters
2. **Physics Compute**: PhysicsComputeNode writes 3D positions to model matrix translation (column 3)
3. **Graphics Rendering**: EntityGraphicsNode reads model matrices for MVP transformation
4. **Vertex Shader**: Applies Object → World (model) → View → Projection transformation

### Resource Dependencies
- **EntityComputeNode**: ReadWrite entity buffer (movement parameters)
- **PhysicsComputeNode**: ReadWrite entity buffer + model matrix buffer
- **EntityGraphicsNode**: Read entity buffer + model matrix buffer

### Performance Optimizations
- **Eliminated Position Ping-Pong**: Single model matrix buffer replaces 4 position buffers
- **Cache-Optimized Access**: Model matrix layout matches GPU cache line boundaries
- **Reduced Memory Barriers**: Simplified compute-graphics synchronization
- **Template Code Elimination**: BaseComputeNode reduces duplication by 300+ lines