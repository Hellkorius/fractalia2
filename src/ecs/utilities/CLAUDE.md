# ECS Utilities

## Purpose
Essential utility library supporting the 80,000+ entity ECS pipeline with debugging, profiling, and system constants.

## Core Components

### constants.h - System Constants
```cpp
namespace SystemConstants {
    constexpr size_t DEFAULT_ENTITY_BATCH_SIZE = 1000;
    constexpr size_t MIN_ENTITY_RESERVE_COUNT = 1000;
    constexpr int MOVEMENT_TYPE_RANDOM_WALK = 0;
}
```
**Usage**: Entity batch processing, GPU buffer sizing, shader movement type IDs

### debug.h - Conditional Logging
```cpp
DEBUG_LOG(message);  // Zero overhead in release builds
```
**Pattern**: Stream-style logging that compiles away when `NDEBUG` defined

### profiler.h - Performance Analysis
**Key Classes**:
- `ProfileTimer`: High-resolution timing (ms/μs precision)
- `ProfileScope`: RAII automatic timing
- `Profiler`: Thread-safe singleton with statistics aggregation

**Essential API**:
```cpp
PROFILE_SCOPE("SystemName");     // RAII scope timing
PROFILE_FUNCTION();              // Profile current function
PROFILE_BEGIN_FRAME();           // Frame timing start
PROFILE_END_FRAME();             // Frame timing end + warnings
Profiler::getInstance().printReport();  // Console statistics
```

## Data Flow
```
Code Execution → ProfileScope → Timer → Statistics → Reports
Entity Processing ← Constants ← Compile-time substitution
Debug Messages → Conditional compilation → Console output
```

## Integration Points

### Service Layer
- **ControlService**: Heavy DEBUG_LOG usage, performance stats display
- **ServiceLocator**: Service initialization timing

### ECS Systems
- **systems_common.h**: Imports all utilities as shared headers
- **Entity systems**: Use constants for batch sizes and processing limits

### GPU Pipeline
- **GPUEntityManager**: Buffer sizing with batch constants
- **EntityBufferManager**: GPU upload batch processing

## Critical Patterns

### Performance Monitoring
```cpp
{
    PROFILE_SCOPE("CriticalSystem");
    // System execution code
}  // Automatic timing on scope exit
```

### Debug Logging
```cpp
DEBUG_LOG("Entity count: " << entityCount << " fps: " << fps);
// Compiles to no-op in release builds
```

### Batch Processing
```cpp
entities.reserve(SystemConstants::MIN_ENTITY_RESERVE_COUNT);
processBatch(entities, SystemConstants::DEFAULT_ENTITY_BATCH_SIZE);
```

## Thread Safety
- **Profiler**: Mutex-protected for multi-threaded scenarios
- **Constants**: Thread-safe (immutable compile-time)
- **Debug**: Main thread only (std::cout limitation)

## Performance Impact
- **Profiling**: ~5-10% overhead when enabled; disable for production
- **Debug**: Zero cost in release builds
- **Constants**: Zero runtime cost (compile-time substitution)