# AAA-Grade ECS Architecture for Fractalia2

## Overview

The Fractalia2 ECS architecture has been upgraded to AAA-grade standards with a fully modular, service-oriented design that maintains the engine's 80,000+ entity performance requirements while providing enterprise-level code organization and maintainability.

## Architecture Layers

### 1. Core Infrastructure (`src/ecs/core/`)

**Service Locator Pattern**
- `service_locator.h` - Dependency injection system with type-safe service registration
- Template-based service management with C++20 concepts
- Thread-safe service access and lifecycle management

**World Management**
- `world_manager.h/cpp` - Central ECS world coordination
- Dynamic module loading/unloading with dependency management
- Performance monitoring and frame execution coordination
- Integration with Flecs ECS with REST API support

### 2. Services Layer (`src/ecs/services/`)

**High-level Game Services**
- `input_service.h/cpp` - Centralized input management with action-based bindings
- `camera_service.h/cpp` - Multi-camera support with transitions and viewports
- `rendering_service.h/cpp` - ECS-Vulkan bridge with LOD and culling

All services follow the dependency injection pattern and integrate seamlessly with existing systems.

### 3. Event System (`src/ecs/events/`)

**Decoupled Communication**
- `event_bus.h` - High-performance, type-safe event dispatcher
- `event_types.h` - Comprehensive game event definitions
- `event_listeners.h` - RAII event handling utilities

Features priority-based queuing, immediate/deferred processing, and Flecs ECS integration.

### 4. Modular Systems (`src/ecs/modules/`)

**Self-contained Game Modules**
- `input_module.h/cpp` - Complete input processing encapsulation
- `movement_module.h/cpp` - Entity movement and physics coordination
- `rendering_module.h/cpp` - Render preparation and GPU synchronization

Each module can be loaded/unloaded independently with proper cleanup and dependency management.

### 5. Utilities (`src/ecs/utilities/`)

**Development and Performance Tools**
- `component_queries.h` - Optimized ECS component queries and filters
- `system_helpers.h` - System development utilities and registration helpers
- `performance_tools.h` - Comprehensive profiling and monitoring tools

## Key Features

### Modularity
- **Plugin Architecture**: Modules can be loaded/unloaded at runtime
- **Service Dependencies**: Clean dependency injection without tight coupling
- **Event-driven Communication**: Loose coupling through type-safe events

### Performance
- **Zero-overhead Abstractions**: Template-based design with compile-time optimizations
- **Cache-friendly Design**: Maintains existing 128-byte GPU entity optimization
- **Minimal Runtime Cost**: Service locator with compile-time type resolution

### Developer Experience
- **Fluent APIs**: Easy-to-use builder patterns and convenience functions
- **Comprehensive Tooling**: Built-in profiling, monitoring, and debugging utilities
- **Type Safety**: C++20 concepts and template metaprogramming for compile-time checks

### Scalability
- **80,000+ Entity Support**: Maintains existing performance requirements
- **Multi-threaded Safe**: Thread-safe service access and event processing
- **Memory Efficient**: Minimal overhead while providing enterprise features

## Integration with Existing Systems

### Flecs ECS Integration
- Maintains existing component definitions and system phases
- Seamless integration with existing Transform, Renderable, MovementPattern components
- Phase-based system execution (PreInput → Input → Logic → Physics → Render → PostRender)

### Vulkan Renderer Integration
- Preserves existing GPUEntityManager and GPU-driven rendering
- RenderingService bridges ECS and Vulkan without performance loss
- Maintains 128-byte cache-optimized GPUEntity structure

### SDL3 Input Integration
- InputService wraps existing input_system functionality
- Backwards compatible with existing input handling
- Adds action-based input mapping and context switching

## Usage Examples

### Service Registration
```cpp
// Initialize services
auto worldManager = ServiceLocator::instance().createAndRegister<WorldManager>();
auto inputService = ServiceLocator::instance().createAndRegister<InputService>();
auto cameraService = ServiceLocator::instance().createAndRegister<CameraService>();

// Initialize
worldManager->initialize();
inputService->initialize(world, window);
cameraService->initialize(world);
```

### Module Loading
```cpp
// Load modules
auto inputModule = worldManager->loadModule<InputModule>("input");
auto movementModule = worldManager->loadModule<MovementModule>("movement");
auto renderingModule = worldManager->loadModule<RenderingModule>("rendering");

// Execute frame
worldManager->executeFrame(deltaTime);
```

### Event System Usage
```cpp
// Subscribe to events
auto listener = EventBus::instance().subscribe<EntityCreatedEvent>(
    [](const EntityCreatedEvent& event) {
        std::cout << "Entity created: " << event.entityId << std::endl;
    });

// Publish events
EventBus::instance().publish(EntityCreatedEvent{entity.id(), glm::vec3(0, 0, 0)});
```

### Performance Monitoring
```cpp
PerformanceTools::PerformanceMonitor monitor;
monitor.enable();
monitor.enableLogging("performance.csv");

// In game loop
monitor.startFrame();
worldManager->executeFrame(deltaTime);
monitor.printReport();  // Periodic reporting
```

## Migration Path

The architecture is designed for **incremental adoption**:

1. **Phase 1**: Service registration for existing systems
2. **Phase 2**: Module encapsulation of related systems
3. **Phase 3**: Event system integration for loose coupling
4. **Phase 4**: Full modular architecture with dynamic loading

Each phase maintains backwards compatibility while providing increased modularity and maintainability.

## Performance Characteristics

- **Zero-overhead Services**: Template-based service locator with compile-time resolution
- **Minimal Event Overhead**: Type-erased events with efficient dispatch
- **Cache-friendly Modules**: Maintains existing component layout optimizations
- **Scalable Architecture**: Designed for 80,000+ entities at 60+ FPS

The architecture provides AAA-grade modularity while preserving the high-performance characteristics required for the Fractalia2 engine's demanding requirements.