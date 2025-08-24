# ECS Systems

(CPU-side system infrastructure for entity lifecycle and movement coordination with GPU compute)

## Files

### systems_common.h
**Inputs:** None (header aggregation)  
**Outputs:** Common includes for Flecs ECS, components, utilities, profiler, and GPU entity management across system files.

### lifetime_system.h
**Inputs:** Flecs entity and component headers  
**Outputs:** Function signature for lifetime system processing.

### lifetime_system.cpp
**Inputs:** Flecs entity with Lifetime component, world delta time  
**Outputs:** Updated Lifetime component age, entity destruction when maxAge exceeded.

### movement_system.h
**Inputs:** Systems common headers, Flecs world  
**Outputs:** Movement phase registration functions, statistics structures, and GPU-CPU coordination interface.

### movement_system.cpp
**Inputs:** Flecs world, MovementPattern and Velocity components, ECS observers  
**Outputs:** ECS execution phases (Movement, Physics, Sync), component statistics tracking, movement pattern reset functionality.