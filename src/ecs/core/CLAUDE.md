# ECS Core Infrastructure

## Purpose
Core service-based architecture for Entity-Component-System framework. Provides dependency injection, ECS world management, and entity lifecycle controls for high-performance rendering pipeline (80k+ entities at 60 FPS).

## Key Components

### ServiceLocator (service_locator.h)
**Pattern**: Singleton dependency injection container with lifecycle management
- **Registration**: `createAndRegister<ServiceType>(name, priority, args...)` 
- **Access**: `getService<T>()` or `requireService<T>()` for exceptions
- **Dependencies**: `declareDependencies<Service, Dep1, Dep2...>()`
- **Lifecycle**: Priority-based init (higher first), reverse-order shutdown
- **Thread Safety**: Mutex-protected access with lifecycle validation

```cpp
// Registration with dependency injection
auto worldManager = ServiceLocator::instance().createAndRegister<WorldManager>("WorldManager", 100);
ServiceLocator::instance().declareDependencies<InputService, WorldManager>();

// Access via convenience macro
SERVICE(InputService).processInput();
```

### WorldManager (world_manager.h/.cpp)
**Pattern**: ECS world orchestration with modular system loading
- **Core**: Flecs world management with multi-threading support
- **Modules**: Template-based loading with `loadModule<ModuleType>(name, args...)`
- **Performance**: 60-frame averaging, entity counting, FPS monitoring
- **Execution**: `executeFrame(deltaTime)` with exception recovery
- **Lifecycle**: Dependency-aware module unloading in reverse order

```cpp
// Module loading with validation
auto movementModule = worldManager.loadModule<MovementModule>("Movement");
worldManager.executeFrame(deltaTime); // Frame execution with monitoring
```

### EntityFactory (entity_factory.h)
**Pattern**: Builder pattern with object pooling for entity creation
- **Builder**: Fluent API with `.at(pos).withColor(color).asDynamic().build()`
- **Pooling**: Automatic entity reuse for dynamic scenarios
- **Batch**: `createBatch(count, configureLambda)` for swarm creation
- **Presets**: `createSwarm()`, `createMovingEntity()` with movement patterns
- **Performance**: Pre-allocated pools, GPU-compatible component layout

```cpp
// Fluent entity creation
auto entity = factory.create()
    .at(glm::vec3(x, y, z))
    .withColor(1.0f, 0.5f, 0.2f, 1.0f)
    .onLayer(1)
    .asDynamic()
    .build();

// Batch swarm creation
auto entities = factory.createSwarm(1000, center, radius);
```

## Data Flow

### Service Architecture Pattern
```
ServiceLocator (Priority Order)
├── WorldManager (100) → Flecs world + module loading
├── InputService (90) → Action-based input processing  
├── CameraService (80) → Multi-camera with transitions
├── RenderingService (70) → Render queue + culling
└── ControlService (60) → Game logic coordination
```

### Entity Lifecycle
```
EntityFactory → EntityBuilder → Flecs Entity → Components → GPU Sync
     ↓              ↓              ↓           ↓          ↓
   Pooling    Fluent Config   ECS Storage  Transform  GPUEntity
```

### Frame Execution
```
WorldManager.executeFrame()
├── Module updates (custom logic)
├── Flecs world.progress() (ECS systems)
├── Performance monitoring (60-frame average)
└── Exception recovery + metrics
```

## Critical Integration Points

### ECS Module Pattern
- **Base**: `ECSModule` abstract class with `initialize()`, `shutdown()`, `update()`
- **Loading**: Template constraints ensure proper inheritance
- **Management**: WorldManager tracks lifecycle and dependencies

### Component Bridge
- **Transform**: Position/rotation/scale with cached matrix calculation
- **Renderable**: Color/layer/visibility with GPU upload tracking  
- **MovementPattern**: GPU compute parameters (amplitude, frequency, phase)
- **Tags**: Static/Dynamic/Pooled for optimization hints

### Service Dependencies
- Services declare dependencies via template metaprogramming
- Validation occurs before initialization to prevent runtime failures
- Lifecycle states prevent access to uninitialized services

## Performance Considerations

### Memory Management
- Entity pooling reduces allocation overhead for dynamic entities
- Component layout optimized for cache coherency (Transform, Renderable)
- Service instances cached to avoid repeated map lookups

### GPU Integration
- EntityFactory generates GPU-compatible movement patterns
- Component changes tracked for selective GPU uploads
- Batch operations minimize CPU→GPU transfer overhead

### Threading
- ServiceLocator provides thread-safe access with mutex protection
- Flecs world configured for multi-threading based on hardware
- Frame execution isolated for exception recovery

---
**Dependencies**: Flecs ECS, GLM (math), SDL3 (events), Vulkan (GPU sync)