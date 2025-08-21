# Vulkan Resources Architecture

## Overview
Resource management subsystem providing unified access to Vulkan buffers, images, memory allocation, and descriptors through a coordinated facade pattern that eliminates circular dependencies and delegation overhead.

## Core Architecture

### ResourceCoordinator (Central Hub)
Primary entry point for all resource operations - delegates to specialized managers:
- **Resource Creation**: Buffers, images, image views via ResourceFactory
- **Memory Management**: Allocation, mapping, pressure detection via MemoryAllocator  
- **Transfer Operations**: CPU→GPU data transfers via TransferManager
- **Graphics Resources**: Uniform buffers, vertex/index data via GraphicsResourceManager
- **Descriptor Management**: Pool and set allocation via DescriptorPoolManager

### Resource Handle System
```cpp
struct ResourceHandle {
    vulkan_raii::Buffer buffer;           // RAII buffer wrapper
    vulkan_raii::Image image;             // RAII image wrapper  
    vulkan_raii::ImageView imageView;     // RAII image view wrapper
    vulkan_raii::DeviceMemory memory;     // RAII memory wrapper
    void* mappedData;                     // Persistent mapping pointer
    VkDeviceSize size;                    // Allocation size
};
```

## Key Subsystems

### buffers/ - Buffer Management
**BufferManager**: Facade coordinating specialized buffer components:
- **StagingBufferPool**: Temporary upload staging with region allocation
- **BufferRegistry**: GPU buffer lifecycle and tracking
- **TransferOrchestrator**: Batch transfer operations optimization
- **BufferStatisticsCollector**: Memory usage and performance monitoring

### core/ - Resource Coordination
**ResourceFactory**: Pure resource creation (SRP compliance)
- Buffer/image creation with memory binding
- Resource destruction with proper cleanup ordering

**MemoryAllocator**: VMA wrapper for memory management
- Memory pressure detection and recovery
- Unified mapping interface for all resource types
- Statistics tracking with fragmentation analysis

**CommandExecutor**: Command buffer management for transfers
- Async transfer support with completion tracking
- Queue family optimization for transfer operations

### descriptors/ - Descriptor Management
**DescriptorSetManagerBase**: Common descriptor management patterns (DRY principle)
- Template method pattern for specialized implementations
- Shared pool management and validation
- Lifecycle coordination with proper cleanup ordering

### managers/ - High-Level Resource Managers
**GraphicsResourceManager**: Consolidated graphics pipeline resources
- Uniform buffer creation and management
- Vertex/index buffer handling
- Descriptor set allocation and updates with unified binding system

**DescriptorPoolManager**: Descriptor pool allocation and recycling
- Pool size optimization based on usage patterns
- Automatic pool expansion under pressure

## Data Flow Patterns

### Resource Creation
1. **Request** → ResourceCoordinator
2. **Delegate** → ResourceFactory for creation
3. **Allocate** → MemoryAllocator for memory binding  
4. **Return** → ResourceHandle with RAII wrappers

### Buffer Upload
1. **Request** → BufferManager
2. **Stage** → StagingBufferPool allocation
3. **Transfer** → TransferOrchestrator batch optimization
4. **Execute** → CommandExecutor async operations

### Memory Management
1. **Monitor** → MemoryAllocator pressure detection
2. **Recover** → Automatic resource optimization
3. **Track** → Statistics collection for performance analysis

## RAII Integration
All Vulkan resources use RAII wrappers from `vulkan_raii.h`:
- **Automatic cleanup** on scope exit or exception
- **Move semantics** for efficient resource transfer
- **Explicit cleanup ordering** via `cleanupBeforeContextDestruction()`

## Usage Patterns

### Basic Resource Creation
```cpp
ResourceCoordinator coordinator;
auto bufferHandle = coordinator.createBuffer(size, usage, properties);
coordinator.copyToBuffer(bufferHandle, data, size);
```

### Advanced Buffer Operations
```cpp
BufferManager& buffers = coordinator.getBufferManager();
auto staging = buffers.allocateStagingGuarded(size);
buffers.copyToBufferAsync(bufferHandle, data, size);
```

### Graphics Resource Management
```cpp
GraphicsResourceManager& graphics = coordinator.getGraphicsManager();
graphics.createAllGraphicsResources();
graphics.updateDescriptorSetsWithEntityAndPositionBuffers(entityBuffer, posBuffer);
```

## Performance Characteristics
- **Zero-copy staging** with persistent mapping
- **Batch transfer optimization** for multiple operations
- **Memory pressure detection** with automatic recovery
- **RAII overhead elimination** through move semantics
- **Cache-friendly resource handles** (128-byte alignment)