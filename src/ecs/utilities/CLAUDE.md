# ECS Utilities Directory Reference

## Purpose
Core utility library for Flecs-based Entity Component System operations. Provides debugging tools, high-resolution profiling, and system-wide constants for the 80,000+ entity GPU-driven rendering pipeline.

## File Hierarchy
```
src/ecs/utilities/
├── constants.h             # SystemConstants namespace with compile-time ECS constants
├── debug.h                 # DEBUG_LOG macro for conditional debug output
├── profiler.h              # ProfileTimer, ProfileScope, Profiler singleton with thread-safe timing
└── CLAUDE.md               # This reference file
```

## Inputs & Outputs by Component

### constants.h
**Inputs:** None (compile-time constants only)

**Outputs:**
- `SystemConstants::DEFAULT_ENTITY_BATCH_SIZE = 1000` - Entity processing batch size
- `SystemConstants::MIN_ENTITY_RESERVE_COUNT = 1000` - Minimum entity pool reservation  
- `SystemConstants::MOVEMENT_TYPE_RANDOM_WALK = 0` - GPU shader movement type identifier

**Integration:** Used in systems_common.h, entity factories, and GPU buffer management

### debug.h  
**Inputs:**
- Debug messages via `DEBUG_LOG(message)` macro
- Compile flag `NDEBUG` controls compilation behavior

**Outputs:**
- `std::cout << message << std::endl` in debug builds
- Complete no-op (optimized away) in release builds

**Integration:** Used extensively in control_service.cpp, systems_common.h, main.cpp for conditional logging

### profiler.h
**Classes:**
- `ProfileTimer`: High-resolution timer with start/stop/getMilliseconds/getMicroseconds
- `ProfileScope`: RAII wrapper for automatic scope-based timing  
- `Profiler`: Thread-safe singleton for performance data collection and reporting

**Inputs:**
- Scope names (`std::string`) for categorized timing
- Memory usage bytes via `updateMemoryUsage(size_t)`
- Frame timing via `beginFrame()/endFrame()` calls
- Manual timing via `ProfileTimer` start/stop operations

**Outputs:**
- `ProfileReport` structures with min/max/average/recent timing data
- Console performance reports with timing tables and percentages
- CSV export capabilities for external analysis
- Performance warnings for frames exceeding target (16.67ms default)
- Memory usage tracking (current/peak in MB)
- Frame count statistics

**Data Flow:**
```
Scope creation → Timer start → Code execution → Scope destruction → 
Timer stop → Statistics aggregation → Report generation (on demand)
```

**API Surface:**
- `PROFILE_SCOPE(name)` - RAII scope profiling
- `PROFILE_FUNCTION()` - Profile current function
- `PROFILE_BEGIN_FRAME()/PROFILE_END_FRAME()` - Frame timing
- `Profiler::getInstance().printReport()` - Console output
- `Profiler::getInstance().exportToCSV(filename)` - File export

## Peripheral Dependencies & Integration

### Active Integration Points
**main.cpp**: Uses DEBUG_LOG and Profiler for application-level logging and frame timing
**control_service.cpp**: Heavy DEBUG_LOG usage, Profiler for performance stats display
**systems_common.h**: Imports all three utilities as common headers for ECS systems

### ECS Service Layer (../services/)
- **ServiceLocator**: Dependency injection - profiler used for service initialization timing
- **ControlService**: Uses DEBUG_LOG extensively, Profiler for performance statistics display
- **CameraService, RenderingService**: Potential profiling integration points (not yet implemented)

### ECS Systems (../systems/)
- **systems_common.h**: Provides utilities as shared headers for system implementations
- **Movement/Lifetime Systems**: Use constants for batch sizes and entity processing limits

### GPU Pipeline (../gpu/)
- **GPUEntityManager**: Uses constants for buffer sizing and batch processing
- **EntityBufferManager**: Batch size constants for GPU upload operations

### Flecs Integration
- World entity counting for profiler statistics
- Component queries for performance analysis
- System execution timing via ProfileScope integration

## Key Notes & Conventions

### Performance Impact
- **Profiling Overhead**: ~5-10% performance cost when enabled; disable for production
- **Debug Logging**: Zero overhead in release builds (NDEBUG optimization)
- **Constants**: Zero runtime cost (compile-time substitution)

### Usage Patterns
- **Debug Logging**: `DEBUG_LOG("Message: " << variable)` - stream-style logging
- **Scope Profiling**: `PROFILE_SCOPE("SystemName")` at function entry
- **Constants**: `SystemConstants::DEFAULT_ENTITY_BATCH_SIZE` for consistent sizing

### Thread Safety
- **Profiler**: Mutex-protected for multi-threaded timing scenarios
- **Debug Macros**: Main thread only (std::cout not thread-safe)
- **Constants**: Thread-safe (immutable compile-time values)

### Memory Management
- **Profiler**: Automatic cleanup via singleton pattern
- **ProfileScope**: RAII ensures timer cleanup on scope exit
- **Debug Macros**: No dynamic allocation

---
**Note**: If ECS components, service APIs, Flecs integration patterns, or GPU pipeline architecture change, this documentation must be updated to maintain accuracy.