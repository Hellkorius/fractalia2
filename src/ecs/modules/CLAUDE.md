# ECS Modules Directory

## Purpose
Self-contained ECS modules that encapsulate domain-specific systems, phases, and component processing. Each module handles initialization, system registration, phase setup, and cleanup for a specific subsystem (input, movement) within the Flecs ECS framework.

**Note:** Rendering functionality has been integrated directly into RenderingService (see `../services/rendering_service.*`) for better architectural cohesion.

## File/Folder Hierarchy
```
/home/nar/projects/fractalia2render/src/ecs/modules/
├── input_module.h          # Input processing module interface
├── input_module.cpp        # SDL event processing, input state management
├── movement_module.h       # Movement and physics module interface  
└── movement_module.cpp     # Random walk patterns, velocity physics
```

## Inputs & Outputs (by Component)

### InputModule
**Inputs:**
- `SDL_Window*` - Window context for SDL event processing
- `flecs::world&` - ECS world instance for system registration
- SDL events via `InputManager::processSDLEvents()`

**Outputs:**
- Creates `InputState`, `KeyboardInput`, `MouseInput`, `InputEvents` components
- Registers input processing system in `InputPhase` → `InputEventsPhase`
- Provides input query interface: `isKeyDown()`, `getMousePosition()`, etc.
- Updates singleton input entity with current frame input state

**Key Dependencies:**
- `../core/world_manager.h` - ECS module base class
- `../systems/input_system.h` - System callback functions
- `../components/component.h` - Input-related components

### MovementModule  
**Inputs:**
- `GPUEntityManager*` - Bridge for CPU→GPU entity synchronization
- `flecs::world&` - ECS world for system registration
- Frame `deltaTime` for physics calculations

**Outputs:**
- Processes entities with `Transform`, `MovementPattern`, `Velocity` components
- Updates entity positions via random walk calculations (sine wave patterns)
- Applies velocity-based physics with damping
- Registers systems in `MovementPhase` → `PhysicsPhase` → `MovementSyncPhase`
- Synchronizes movement data with GPU via `GPUEntityManager::addEntitiesFromECS()`

**Key Dependencies:**
- `../gpu_entity_manager.h` - CPU→GPU synchronization
- `../components/component.h` - Transform, MovementPattern, Velocity components

## Data Flow Between Components

### ECS Module Lifecycle
1. **Initialization**: `WorldManager::loadModule()` → `module->initialize(world)`
2. **Phase Setup**: Each module creates dependent ECS phases (`InputPhase`, `MovementPhase`, etc.)
3. **System Registration**: Systems registered to specific phases with component queries
4. **Frame Update**: `module->update(deltaTime)` calls domain-specific processing
5. **Cleanup**: `module->shutdown()` destroys systems and resets state

### Inter-Module Data Flow
```
InputModule (SDL events) 
    ↓ (input components)
MovementModule (physics calculations)
    ↓ (updated transforms)  
RenderingService (render preparation and GPU sync)
    ↓ (via service layer)
VulkanRenderer (frame rendering)
```

### Component Processing Pipeline
1. **Input**: SDL events → `InputState` components
2. **Movement**: `Transform` + `MovementPattern` → updated positions
3. **Physics**: `Transform` + `Velocity` → applied velocity with damping
4. **Rendering**: RenderingService handles `Transform` + `Renderable` → model matrices, culling data, LOD levels
5. **GPU Sync**: RenderingService → `GPUEntityManager` → GPU buffers

## Peripheral Dependencies

### Core ECS Infrastructure
- `/home/nar/projects/fractalia2render/src/ecs/core/world_manager.h` - Module loading and lifecycle
- `/home/nar/projects/fractalia2render/src/ecs/core/service_locator.h` - Service dependency injection
- `/home/nar/projects/fractalia2render/src/ecs/components/component.h` - Core component definitions

### External Subsystems  
- `/home/nar/projects/fractalia2render/src/ecs/gpu_entity_manager.h` - CPU→GPU bridge
- `/home/nar/projects/fractalia2render/src/vulkan_renderer.h` - Main renderer (rendering module only)
- SDL3 - Window management and input events (input module only)

### Legacy Systems
- `/home/nar/projects/fractalia2render/src/ecs/systems/input_system.h` - System callback functions
- Component definitions: `Transform`, `Renderable`, `MovementPattern`, `Velocity`, `CullingData`, `LODData`

## Key Notes

### Module Integration Pattern
- All modules inherit from `ECSModule` base class via `WorldManager`
- Modules are accessed via `WorldManager::getModule<T>()` or convenience namespaces
- Example: `InputModuleAccess::isKeyDown(world, scancode)` for quick access

### Phase Dependency System
Each module establishes ECS phase dependencies to ensure correct execution order:
- Input processing runs early (`InputPhase` → `InputEventsPhase`)
- Movement runs after input (`MovementPhase` → `PhysicsPhase` → `MovementSyncPhase`)  
- Rendering runs after movement (`RenderPreparePhase` → `CullPhase` → `LODPhase` → `GPUSyncPhase`)

### System Registration Pattern
- Systems registered with component queries: `world_->system<Transform, Renderable>()`
- Systems assigned to specific phases: `.kind(renderPreparePhase)`
- System entities stored for proper cleanup: `renderPrepareSystem_.destruct()`

### Performance Monitoring
All modules provide performance statistics:
- Timing data for update operations
- Entity processing counts  
- Moving averages for frame-to-frame consistency

### Error Handling Convention
- `initialize()` returns `bool` - false on failure triggers `shutdown()`
- Exception handling in update loops with logging (production requires proper logging)
- Guard checks for `initialized_` state and null dependencies

---

**Important**: If any referenced components, systems, or external dependencies change, this documentation must be updated to reflect the new interfaces and data flow patterns.