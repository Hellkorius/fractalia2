# ECS Components

## Folder Hierarchy
```
src/ecs/components/                    (Core ECS data structures for entity composition and GPU synchronization)
├── camera_component.h                 
├── component.h                        
└── entity.h                          
```

## File Descriptions

### camera_component.h
**Inputs:** Screen coordinates, world positions, movement deltas, zoom factors, aspect ratios.
**Outputs:** View/projection matrices, world-to-screen coordinate transformations, visibility checks.
Implements 2D orthographic camera with lazy matrix computation and unbounded zoom capabilities.

### component.h
**Inputs:** GLM vectors/matrices, entity transform data, input events, frame timing data.
**Outputs:** Cached transformation matrices, GPU-ready render data, input state tracking.
Defines core ECS components including Transform, Renderable, MovementPattern, and input handling structures with optimized memory layouts.

### entity.h
**Inputs:** Flecs entity handles, component data from Transform/Renderable/MovementPattern.
**Outputs:** GPU-synchronized entity data structures for CPU-GPU bridge operations.
Provides entity handle aliasing and GPU entity data conversion from Flecs ECS entities.