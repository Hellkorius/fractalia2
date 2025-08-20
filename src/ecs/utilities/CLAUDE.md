# ECS Utilities Directory Reference

## Purpose
Core utility library for Flecs-based Entity Component System operations. Provides debugging tools, profiling capabilities, and system constants.

## File Hierarchy
```
src/ecs/utilities/
├── constants.h             # System-wide ECS constants and batch sizes
├── debug.h                 # Unified debug output macros
└── profiler.h              # High-resolution profiling with scope-based timing
```

## Inputs & Outputs by Component

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
- **Profiling Overhead**: Performance tools add ~5-10% overhead when enabled; disable in production
- **Debug Logging**: DEBUG_LOG macros are compiled out in release builds for zero overhead

### System Architecture Integration
- **Constants**: Use defined constants for consistent batch sizes and entity limits
- **Debug Output**: Use DEBUG_LOG for conditional debug messages
- **Profiling Integration**: Wrap expensive operations with `PROFILE_SYSTEM` or `ProfileScope`

### Thread Safety
- **Profiler**: Thread-safe via mutex protection for multi-threaded profiling scenarios
- **Debug logging**: Not thread-safe; use from main thread only
- **Constants**: Thread-safe (compile-time constants)

---
**Note**: If any referenced components, services, or Flecs APIs change, this documentation must be updated to maintain accuracy.