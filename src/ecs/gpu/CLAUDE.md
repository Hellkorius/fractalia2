# ECS GPU Subsystem

## Purpose
Bridge between CPU ECS components and GPU compute/graphics pipelines for 80,000+ entity rendering at 60 FPS. Implements Structure of Arrays (SoA) memory layout for optimal GPU performance and cache locality.

## Core Architecture

### Data Flow
1. **ECS → SoA**: Convert Flecs components (Transform, Renderable, MovementPattern) to SoA layout
2. **CPU Staging**: Batch entity data in `GPUEntitySoA` structure  
3. **GPU Upload**: Transfer SoA buffers to specialized GPU buffers
4. **Compute/Graphics**: Ping-pong position buffers for async compute → graphics pipeline

### Key Classes

#### GPUEntityManager
**Single responsibility**: Orchestrate CPU-GPU entity data transfer
- Converts ECS entities to SoA layout via `addEntitiesFromECS()`
- Delegates buffer management to `EntityBufferManager`
- Delegates descriptor management to `EntityDescriptorManager`
- Supports up to 131,072 entities max

#### EntityBufferManager  
**Single responsibility**: Coordinate specialized buffer components
- Delegates to specialized buffers (VelocityBuffer, ColorBuffer, etc.)
- Uses `PositionBufferCoordinator` for ping-pong buffers
- Provides typed upload methods for type safety

#### Specialized Buffers (SRP Pattern)
Each buffer class has **single responsibility** for one data type:
- `VelocityBuffer`: velocity.xy, damping, reserved (vec4)
- `MovementParamsBuffer`: amplitude, frequency, phase, timeOffset (vec4)  
- `RuntimeStateBuffer`: totalTime, initialized, stateTimer, entityState (vec4)
- `ColorBuffer`: RGBA color data (vec4)
- `ModelMatrixBuffer`: transformation matrices (mat4)

#### PositionBufferCoordinator
**Single responsibility**: Ping-pong position buffer coordination
- Primary/alternate buffers for async compute-graphics pipeline
- Current/target buffers for interpolation
- Frame-based buffer access: `getComputeWriteBuffer()`, `getGraphicsReadBuffer()`

#### EntityDescriptorManager
**Single responsibility**: Entity-specific descriptor set management
- Compute descriptors: All SoA buffers for movement calculation
- Graphics descriptors: Position + movement params + uniform buffer
- Swapchain recreation support

## SoA Memory Layout

### Hot Data (Cache Line 1: 0-63 bytes)
```cpp
glm::vec4 velocity;        // Frequently accessed in compute
glm::vec4 movementParams;  // Movement calculation parameters  
glm::vec4 runtimeState;    // Shader runtime state
glm::vec4 color;           // Rendering color
```

### Cold Data (Cache Line 2: 64-127 bytes)  
```cpp
glm::mat4 modelMatrix;     // Rarely accessed transform matrix
```

## Buffer Types & Usage

### Compute Pipeline Buffers
- **Input**: velocity, movementParams, runtimeState, currentPosition
- **Output**: Updated position buffer (ping-pong)
- **Shader Bindings**: See `EntityDescriptorBindings::Compute`

### Graphics Pipeline Buffers
- **Input**: position, movementParams (for color), uniform buffer
- **Shader Bindings**: See `EntityDescriptorBindings::Graphics`

## Key Patterns

### Interface Segregation
`IBufferOperations` interface for common buffer operations without coupling to specific buffer types.

### Template Method Pattern  
`BufferBase` provides common buffer lifecycle, specialized classes override specific behavior.

### Composition Over Inheritance
`EntityBufferManager` composes specialized buffers rather than inheriting from monolithic buffer class.

### Ping-Pong Synchronization
Position buffers alternate between compute write and graphics read to prevent CPU-GPU stalls.

## Integration Points

- **Input**: Flecs entities with Transform, Renderable, MovementPattern components
- **Output**: GPU buffers accessible via VkBuffer handles for frame graph
- **Dependencies**: VulkanContext, ResourceCoordinator, VulkanSync
- **Consumers**: Compute and graphics render nodes in frame graph