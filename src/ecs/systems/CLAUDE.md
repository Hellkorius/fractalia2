# ECS Systems Directory

## Purpose
Legacy ECS systems directory containing minimal CPU-side system implementations. Most computation is GPU-driven via compute shaders, with CPU systems providing registration, phase management, and statistics.

## File Hierarchy
```
systems/
├── systems_common.h        # Common headers and includes for all systems
├── lifetime_system.h       # Lifetime management system header
├── lifetime_system.cpp     # Simple entity auto-destruction logic
├── movement_system.h       # Movement system registration and stats interface
└── movement_system.cpp     # Movement phase setup and statistics (CPU movement disabled)
```

## Inputs & Outputs

### systems_common.h
**Inputs:** None (header-only)
**Outputs:** 
- Common includes: flecs.h, component.h, constants.h, debug.h, vulkan_renderer.h
- Dependency consolidation for all system files

### lifetime_system.h/.cpp
**Inputs:**
- `flecs::entity` - Entity reference with world access
- `Lifetime&` - Component with maxAge, currentAge, autoDestroy fields
- `world().delta_time()` - Frame time delta from Flecs world

**Outputs:**
- Entity destruction via `e.destruct()` when lifetime expires
- Updated `Lifetime.currentAge` field
- No buffer writes or GPU communication

### movement_system.h/.cpp
**Inputs:**
- `flecs::world&` - Flecs world instance for system registration
- `GPUEntityManager*` - Optional GPU manager (currently unused)
- Component queries: `MovementPattern`, `Velocity` for statistics

**Outputs:**
- Flecs execution phases: MovementPhase → PhysicsPhase → MovementSyncPhase
- Observer-based entity counting statistics via `MovementStats`
- **No actual movement computation** - delegated to GPU compute shaders
- Phase dependencies for potential future CPU systems

## Data Flow

1. **Registration Flow:**
   - WorldManager calls `MovementSystem::registerSystems(world, nullptr)`
   - Sets up execution phases and observer-based statistics
   - CPU movement systems remain disabled (GPU-driven architecture)

2. **Lifetime Flow:**
   - Flecs queries entities with Lifetime component
   - lifetime_system() called per entity per frame
   - Automatic entity destruction when maxAge reached

3. **Movement Statistics:**
   - Flecs observers track add/remove events for MovementPattern/Velocity
   - Real-time entity counting without per-frame queries
   - Statistics accessible via `getStats()` for monitoring

## Peripheral Dependencies

### Direct Dependencies:
- `/src/ecs/components/component.h` - All ECS component definitions
- `/src/ecs/gpu/gpu_entity_manager.h` - GPU-CPU bridge (interface only)
- `/src/ecs/core/world_manager.cpp` - System registration entry point
- `/src/ecs/utilities/` - Constants, debug utilities, profiler

### GPU Integration:
- Movement computation handled by `/src/shaders/movement_random.comp`
- GPU reads `GPUEntity` data directly, bypasses CPU movement systems
- CPU Transform components used only for initial setup

### Service Layer:
- Services architecture handles high-level coordination
- Systems provide low-level ECS component processing
- Service → System → Component data flow pattern

## Key Notes

### Architecture Transition:
- **CPU movement systems DISABLED** - GPU compute shaders handle all movement
- Phase scaffolding maintained for potential debugging or future CPU systems
- Statistics and lifecycle management remain CPU-side

### Performance Optimizations:
- Observer-based statistics avoid per-frame entity counting queries
- Efficient cache-aligned `GPUEntity` structure (128 bytes, 2 cache lines)
- Lifetime system processes only entities with Lifetime component

### Integration Points:
- Systems registered via WorldManager.registerSystems()
- Component definitions in `/src/ecs/components/component.h`
- GPU synchronization through EntityBufferManager and EntityDescriptorManager

### Debugging Notes:
- CPU movement systems can be re-enabled by uncommenting lines 67-76 in movement_system.cpp
- Use `resetAllMovementPatterns()` to reset movement state for debugging
- Statistics tracking provides real-time entity count monitoring

---
**Note:** If component definitions, GPU entity structure, or service integration patterns change, this file requires updates to maintain accuracy.