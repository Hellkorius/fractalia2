# ECS Systems

## Purpose
CPU side Entity management.

## Key Components

### Lifetime System (`lifetime_system.h/cpp`)
- **Function**: Automatic entity destruction based on age
- **Input**: `Lifetime` component with `maxAge`, `currentAge`, `autoDestroy`
- **Process**: Updates age per frame, destroys entities when `currentAge >= maxAge`
- **Usage**: Simple entity cleanup without manual management

### Movement System (`movement_system.h/cpp`)
- **Function**: ECS phase setup and statistics tracking
- **Phases**: MovementPhase → PhysicsPhase → MovementSyncPhase
- **Statistics**: Observer-based entity counting for `MovementPattern` and `Velocity`
- **Note**: Movement computation handled by GPU compute shaders

## Data Flow

1. **Registration**: WorldManager calls `MovementSystem::registerSystems(world, nullptr)`
2. **Execution**: Only lifetime system runs per-entity per-frame
3. **GPU Movement**: `/src/shaders/movement_random.comp` handles all position updates
4. **Statistics**: Flecs observers track component add/remove events

## Migration Status

### GPU-Driven Architecture
- **CPU**: Entity lifecycle, statistics, initial component setup
- **GPU**: All movement computation via compute shaders
- **Bridge**: GPUEntityManager handles CPU→GPU data transfer

## Essential Files
```
systems_common.h         # Shared includes for all systems
lifetime_system.h/cpp    # Entity auto-destruction
movement_system.h/cpp    # ECS phases + statistics tracking
```

## Key Patterns

### Observer-Based Statistics
```cpp
world.observer<MovementPattern>("MovementStatsObserver")
    .event(flecs::OnAdd)
    .each([](flecs::entity e, MovementPattern&) {
        stats_.entitiesWithMovement++;
    });
```

## Integration Points
- **Registration**: `/src/ecs/core/world_manager.cpp`
- **Components**: `/src/ecs/components/component.h`
- **GPU Bridge**: `/src/ecs/gpu/gpu_entity_manager.h`
- **Services**: Service architecture handles high-level game logic
