# ECS Core

## /home/nar/projects/fractalia2/src/ecs/core
(Provides foundational ECS infrastructure for entity lifecycle, service dependency injection, and world management coordination.)

### entity_factory.h
**Inputs:** Flecs world reference, entity creation parameters (position, color, movement patterns), batch configuration functions.
**Outputs:** Configured EntityBuilder instances, batches of entities with components, pooled entity recycling system.
Implements fluent builder pattern for entity creation with Transform, Renderable, MovementPattern, and tag components.

### service_locator.h
**Inputs:** Service instances, dependency declarations, initialization priorities, lifecycle state changes.
**Outputs:** Thread-safe service registration/retrieval, dependency validation results, ordered service initialization.
Manages service lifecycle with priority-based ordering and validates inter-service dependencies before initialization.

### world_manager.h
**Inputs:** ECS modules, performance monitoring callbacks, frame delta time, system registration requests.
**Outputs:** Flecs world access, module lifecycle management, frame execution coordination, performance metrics.
Coordinates ECS world execution with module loading/unloading and provides performance monitoring integration.

### world_manager.cpp
**Inputs:** Module initialization parameters, frame timing data, system registration requests, performance callbacks.
**Outputs:** Initialized Flecs world with registered systems, executed frame updates, calculated performance metrics.
Implements WorldManager with thread-safe module management and frame-time based performance monitoring.