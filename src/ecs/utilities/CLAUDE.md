# src/ecs/utilities

(Provides system-wide constants, debugging infrastructure, and performance profiling tools for the ECS framework)

## Files

### constants.h
**Inputs:** None (compile-time constants)  
**Outputs:** System-wide constants including entity batch sizes and movement type enumerations. Provides DEFAULT_ENTITY_BATCH_SIZE, MIN_ENTITY_RESERVE_COUNT, and MOVEMENT_TYPE_RANDOM_WALK for consistent configuration across the ECS framework.

### debug.h  
**Inputs:** Preprocessor NDEBUG flag and debug messages via DEBUG_LOG macro  
**Outputs:** Unified debug output control that conditionally outputs debug messages to stdout in debug builds. Provides DEBUG_LOG macro that compiles to no-op in release builds for zero-cost debugging.

### profiler.h
**Inputs:** System calls, timing data, memory usage statistics, and named profiling scopes  
**Outputs:** Comprehensive performance monitoring system with ProfileTimer, ProfileScope RAII wrapper, and singleton Profiler class. Generates detailed performance reports with timing statistics, memory usage tracking, frame rate monitoring, and CSV export capabilities for performance analysis.