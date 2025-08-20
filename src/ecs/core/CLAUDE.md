# ECS Core System Documentation

## Purpose
Foundational infrastructure for Entity-Component-System architecture with service-based dependency injection, modular ECS world management, and entity lifecycle controls. Serves as the central orchestration layer between game logic and GPU rendering systems.

## File/Folder Hierarchy
```
/home/nar/projects/fractalia2render/src/ecs/core/
├── service_locator.h           # Dependency injection and service lifecycle management
├── world_manager.h             # ECS world orchestration and module loading
├── world_manager.cpp           # Implementation of ECS frame execution and performance monitoring
└── entity_factory.h            # Entity builder pattern and batch creation utilities
```

## Inputs & Outputs (by component)

### service_locator.h
**Inputs:**
- Service instances (std::shared_ptr<T>) via registerService<T>() or createAndRegister<T>()
- Service dependencies via declareDependencies<T, Dependencies...>()
- Service priority levels (int) for initialization ordering
- Service lifecycle transitions via setServiceLifecycle<T>()

**Outputs:**
- Service instances via getService<T>() → std::shared_ptr<T>
- Service references via requireService<T>() → T& (throws on failure)
- Service validation results via validateDependencies() → bool
- Service initialization coordination via initializeAllServices() → bool
- Service lifecycle state via getServiceLifecycle<T>() → ServiceLifecycle enum
- Debug information via printServiceStatus() and getServiceCount()

**Key APIs:**
- `ServiceLocator::instance()` - Singleton access
- Template-based service registration with type safety
- Priority-ordered initialization (higher priority = earlier init)
- Reverse-order shutdown for dependency cleanup
- Thread-safe access with std::mutex

### world_manager.h/.cpp
**Inputs:**
- ECS modules (std::shared_ptr<ECSModule>) via loadModule<ModuleType>()
- Module names (std::string) for identification and retrieval
- Performance monitoring callbacks (std::function<void(float)>)
- Frame delta time (float) for ECS progress execution

**Outputs:**
- Flecs world reference via getWorld() → flecs::world&
- Module instances via getModule<ModuleType>() → std::shared_ptr<ModuleType>
- Performance metrics via getEntityCount(), getAverageFrameTime(), getFPS()
- Frame execution coordination via executeFrame(float deltaTime)
- Module lifecycle management via unloadModule() and unloadAllModules()

**Key APIs:**
- Template-based module loading with type constraints (must inherit ECSModule)
- Performance monitoring with 60-frame averaging
- Exception-safe frame execution with recovery
- Dependency-aware module unloading (reverse order)

### entity_factory.h
**Inputs:**
- Flecs world reference (flecs::world&) for entity creation
- Entity configuration lambdas via createBatch()
- Transform data (glm::vec3 position, rotation, scale)
- Rendering data (glm::vec4 color, uint32_t layer)
- Movement patterns (MovementType, center point, amplitude, frequency)
- Entity pool candidates via recycle()

**Outputs:**
- EntityBuilder instances for fluent configuration
- Individual entities via create().build() → flecs::entity
- Batch entities via createBatch() → std::vector<flecs::entity>
- Preconfigured entities via createEntity(), createMovingEntity(), createSwarm()
- Entity pooling for performance optimization

**Key APIs:**
- Fluent builder pattern: .at().withColor().onLayer().build()
- Entity pooling for allocation optimization
- Batch creation with configuration callbacks
- Movement pattern integration with GPU compute system
- Component lifecycle management (Transform, Renderable, MovementPattern)

## Peripheral Dependencies

### ECS Components (/ecs/components/)
- **Transform**: Position, rotation, scale with cached matrix calculation
- **Renderable**: Color, layer, visibility with GPU upload tracking
- **MovementPattern**: GPU compute parameters for entity movement
- **Lifetime, Bounds, Velocity**: Additional entity behaviors
- **Input components**: KeyboardInput, MouseInput, InputEvents for ECS-based input

### ECS Modules (Deprecated)
- Module functionality has been integrated into the service layer
- Movement patterns handled by MovementSystem in conjunction with services

### ECS Services (/ecs/services/)
- **InputService**: High-level input with action mapping and context switching
- **CameraService**: Multi-camera management with viewport support
- **RenderingService**: Render queue with culling and batching optimization
- **ControlService**: Game control logic coordination
- All registered via ServiceLocator with priority-based initialization

### GPU Bridge (/ecs/gpu_entity_manager.*)
- **GPUEntityManager**: CPU→GPU synchronization for 80k+ entities
- **GPUEntity**: Cache-optimized 128-byte structure for GPU compute
- Transforms ECS components into GPU-ready data structures

### External Dependencies
- **Flecs ECS**: Core entity-component-system framework
- **GLM**: Mathematics library for transforms and vectors
- **SDL3**: Window and input event processing
- **Vulkan**: GPU resource management and synchronization

## Key Notes

### Service Architecture Patterns
- Services use DECLARE_SERVICE() macro for type safety
- Priority-based initialization: WorldManager(100) → InputService(90) → CameraService(80) → RenderingService(70) → ControlService(60)
- Dependency injection validates all dependencies before initialization
- Lifecycle tracking prevents access to uninitialized services
- Thread-safe singleton access with mutex protection

### Entity Factory Patterns
- Builder pattern enables fluent entity configuration
- Entity pooling reduces allocation overhead for dynamic scenarios
- Movement patterns are GPU-compatible (RandomWalk type)
- Color assignment deferred to GPU compute for performance
- Batch creation optimized for swarm scenarios (80k+ entities)

### Module Integration
- ECSModule base class enforces initialization/shutdown contract
- Modules register systems and components with Flecs world
- Update order managed by WorldManager frame execution
- Module dependencies handled implicitly through service dependencies

### Error Handling & Recovery
- Service initialization returns bool for validation
- Module loading validates before registration
- Frame execution uses exception safety with recovery
- Resource cleanup follows RAII patterns

### Performance Considerations
- Service lookups cached via references to avoid repeated map access
- Entity factory pre-allocates pools for batch creation
- Performance monitoring averages over 60-frame windows
- GPU synchronization minimizes CPU→GPU transfer overhead

---
**Note**: If any referenced components, services, modules, or GPU systems change their interfaces or data structures, this documentation must be updated to maintain accuracy.