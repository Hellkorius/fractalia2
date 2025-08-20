# ECS Utilities Directory Reference

## Purpose
Core utility library for Flecs-based Entity Component System operations. Provides high-level abstractions for component queries, performance monitoring, system management, and ECS debugging tools.

## File Hierarchy
```
src/ecs/utilities/
├── component_queries.h     # Query builders, cached queries, entity filtering
├── constants.h             # System-wide ECS constants and batch sizes
├── debug.h                 # Unified debug output macros
├── memory_manager.h        # ECS memory statistics and reporting
├── performance_tools.h     # Frame timing, performance monitoring, profiling
├── profiler.h              # High-resolution profiling with scope-based timing
├── system_helpers.h        # System builders, management, and service integration
└── system_scheduler.h      # Flecs phase management and execution pipeline
```

## Inputs & Outputs by Component

### component_queries.h
**Inputs:**
- `flecs::world&` - Active ECS world instance
- Component types via templates (`Transform`, `Renderable`, `MovementPattern`, etc.)
- Filter predicates (`std::function<bool(flecs::entity)>`)
- Entity position queries (`glm::vec3` center, `float` radius)

**Outputs:**
- `Query<Components...>` - Type-safe query wrappers with iteration methods
- `CachedQuery` - Cached entity lists with dirty tracking
- `ComponentView<T>` - Component access interface with CRUD operations
- `EntityFilter` - Filtered entity collections based on predicates
- Common query functions: `renderableEntities()`, `movingEntities()`, `physicsEntities()`

**Data Flow:**
`flecs::world` → Query builders → Component iteration → Entity collections → Filter application

### constants.h
**Inputs:** None (compile-time constants)

**Outputs:**
- `DEFAULT_ENTITY_BATCH_SIZE = 1000` - Standard entity processing batch size
- `MIN_ENTITY_RESERVE_COUNT = 1000` - Minimum entity pool reservation
- `MOVEMENT_TYPE_RANDOM_WALK = 0` - GPU shader movement type constant

### debug.h
**Inputs:**
- Debug messages via `DEBUG_LOG(x)` macro
- Compile-time flag `NDEBUG` for conditional compilation

**Outputs:**
- Console output stream (`std::cout`) in debug builds
- No-op in release builds (optimized away)

### memory_manager.h
**Inputs:**
- `flecs::world&` - ECS world for component counting
- Component types: `Transform`, `Renderable`, `Velocity`, `Bounds`, `Lifetime`, `MovementPattern`

**Outputs:**
- `MemoryStats` struct with component counts and memory usage
- Console memory reports via `printMemoryReport()`
- Estimated memory consumption per component type
- Active entity count approximation

**Data Flow:**
`flecs::world.count<Component>()` → Memory calculations → Statistics aggregation → Formatted reporting

### performance_tools.h
**Inputs:**
- Frame timing via `FrameTimer::tick()`
- System profiling via `PerformanceMonitor::startSystem()/endSystem()`
- ECS world for system analysis (`ECSPerformanceAnalyzer`)
- Optional CSV filename for data export

**Outputs:**
- Frame statistics (FPS, min/max/average frame times)
- System performance metrics (execution time per system)
- CSV performance logs with timestamps
- Console performance reports
- Memory allocation tracking

**Data Flow:**
System execution → Timing capture → Statistical analysis → Report generation → CSV export

### profiler.h
**Inputs:**
- Profiling scope names (`std::string`)
- Manual timing via `ProfileTimer` start/stop
- Memory usage updates (`updateMemoryUsage()`)
- Frame timing data

**Outputs:**
- `ProfileReport` structures with timing statistics
- Singleton profiler instance access
- RAII scope profiling via `ProfileScope`
- Performance warnings for slow frames
- CSV export of profiling data
- Console performance reports

**Data Flow:**
Scope entry → Timer start → Code execution → Timer stop → Statistics aggregation → Report generation

### system_helpers.h
**Inputs:**
- `flecs::world&` - ECS world for system registration
- System names (`std::string`)
- Phase entities from Flecs
- Component types via templates
- Service types via `ServiceLocator`
- Lambda functions for system logic

**Outputs:**
- `SystemBuilder<Components...>` - Fluent system creation interface
- `SystemManager` - System lifecycle management (enable/disable/remove)
- `PerformanceProfiler` - System-specific timing analysis
- `ServiceSystemBuilder<Service>` - Service-integrated system creation
- Pre-built common systems (transform update, lifetime management, velocity integration)

**Data Flow:**
System definition → Builder configuration → Flecs registration → Phase assignment → Execution management

### system_scheduler.h
**Inputs:**
- `flecs::world&` - ECS world instance
- Frame delta time (`float`)
- Performance monitoring flags

**Outputs:**
- Phase entities: `PreInput`, `Input`, `Logic`, `Physics`, `Render`, `PostRender`
- Ordered system execution pipeline
- Global singleton components (`ApplicationState`)
- Frame execution coordination
- Performance monitoring integration

**Data Flow:**
Phase creation → System ordering → Frame execution → Global state updates → Performance tracking

## Peripheral Dependencies

### Core ECS Components (../components/)
- **Transform**: Position, rotation, scale with cached matrix calculation
- **Renderable**: Rendering data with color, layer, visibility flags
- **MovementPattern**: GPU-compatible movement parameters
- **Velocity**: Linear and angular velocity vectors
- **Camera**: 2D orthographic camera with zoom and rotation
- **Input Components**: Keyboard, mouse, and event handling
- **Lifetime**: Entity lifecycle management
- **Tag Components**: Static, Dynamic, KeyboardControlled markers

### Service Layer (../services/)
- **ServiceLocator**: Dependency injection for system-service integration
- **InputService**, **CameraService**, **RenderingService**: High-level game services

### Flecs ECS Framework
- World management and entity lifecycle
- Component storage and retrieval
- System registration and execution phases
- Query and iteration APIs

## Key Notes & Conventions

### Performance Considerations
- **Cached Queries**: Use `CachedQuery` for frequently accessed entity sets; invalidate on component changes
- **Batch Processing**: Utilize `DEFAULT_ENTITY_BATCH_SIZE` for optimal memory locality
- **Profiling Overhead**: Performance tools add ~5-10% overhead when enabled; disable in production
- **Memory Tracking**: Memory statistics are estimates based on component sizeof() calculations

### Query Patterns
- **Type Safety**: All queries are templated for compile-time type checking
- **Common Queries**: Pre-built queries in `CommonQueries` namespace for standard entity types
- **Filtering**: Chain filters for complex entity selection (position-based, layer-based, visibility)
- **Iteration**: Prefer `each()` for single entities, `iter()` for batch processing

### System Architecture Integration
- **Phase Dependencies**: Systems must respect phase ordering: PreInput → Input → Logic → Physics → Render → PostRender
- **Service Integration**: Use `ServiceSystemBuilder` for systems requiring service dependencies
- **Error Handling**: System failures are logged but don't halt frame execution
- **Profiling Integration**: Wrap expensive systems with `PROFILE_SYSTEM` macro for automatic timing

### Thread Safety
- **Single-Threaded**: All utilities assume single-threaded ECS access
- **Profiler**: Thread-safe via mutex protection for multi-threaded profiling scenarios
- **Query Caching**: Cache invalidation is not thread-safe; use from main thread only

### GPU Integration Notes
- Component queries interface with GPU entity upload pipeline
- Movement pattern constants must match GPU compute shader definitions
- Memory manager tracks CPU-side component memory only (GPU buffers tracked separately)

---
**Note**: If any referenced components, services, or Flecs APIs change, this documentation must be updated to maintain accuracy.