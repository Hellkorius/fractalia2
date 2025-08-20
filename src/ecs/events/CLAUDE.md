# ECS Events System

## Purpose
Header-only event system designed for high-performance ECS integration with Flecs. Provides type-safe event publishing/subscription with thread-safe processing modes, priority handling, and automatic RAII lifecycle management.

## File/Folder Hierarchy
```
events/
├── event_types.h       # Event data structures and utility functions
├── event_bus.h         # Core event bus implementation with threading support
├── event_listeners.h   # RAII wrappers and ECS integration utilities
└── CLAUDE.md           # This documentation
```

## Inputs & Outputs

### event_types.h
**Inputs:**
- Depends on: `../components/component.h`, `<glm/glm.hpp>`, `<flecs.h>`, `<SDL3/SDL.h>`
- Uses ECS components: `KeyboardInput`, `MouseInput` (from component.h)
- Uses SDL types: `SDL_Event`, `SDL_BUTTON_*`, `SDL_KMOD_*` constants

**Outputs:**
- **Input Events**: `KeyboardEvent`, `MouseButtonEvent`, `MouseMotionEvent`, `MouseWheelEvent`, `TextInputEvent`, `InputStateEvent`
- **Entity Lifecycle**: `EntityCreatedEvent`, `EntityDestroyedEvent`, `ComponentAddedEvent`, `ComponentRemovedEvent`, `ComponentChangedEvent`
- **Physics Events**: `CollisionEvent`, `TriggerEvent`, `PhysicsStepEvent`
- **Camera Events**: `CameraPositionChangedEvent`, `CameraRotationChangedEvent`, `CameraZoomChangedEvent`, `CameraViewChangedEvent`, `CameraBoundsChangedEvent`
- **System Events**: `SystemInitializedEvent`, `SystemShutdownEvent`, `ModuleLoadedEvent`, `ModuleUnloadedEvent`
- **Rendering Events**: `FrameStartEvent`, `FrameEndEvent`, `RenderPassEvent`, `SwapchainRecreatedEvent`
- **Performance Events**: `PerformanceWarningEvent`, `MemoryPressureEvent`
- **Application Events**: `ApplicationStartedEvent`, `ApplicationQuitEvent`, `WindowResizeEvent`, `WindowFocusEvent`
- **Future Extensions**: `AudioEvent`, `NetworkEvent`
- **Debug Events**: `DebugMessageEvent`, `ProfilerEvent`
- **Utility Functions**: `Events::Utility::convertSDLEvent()`, `publishEvent()`, `publishEvents()`

### event_bus.h
**Inputs:**
- Template parameters: EventType (any user-defined event struct)
- Handler functions: `std::function<void(const Event<T>&)>` or `std::function<void(const T&)>`
- Filter functions: `std::function<bool(const Event<T>&)>`
- Processing modes: `Immediate`, `Deferred`, `Conditional`
- Priority levels: `Immediate`, `High`, `Normal`, `Low`, `Deferred`

**Outputs:**
- **EventBus class**: Thread-safe event publishing and subscription
- **Event<T> wrapper**: Type-safe event container with metadata (priority, timestamp, sequenceId)
- **EventListenerHandle**: RAII handle for automatic unsubscription
- **BaseEvent interface**: Type-erased base for event queue storage
- **QueuedEvent**: Priority queue entry for deferred processing
- **EventListener struct**: Internal listener container with filtering and lifecycle
- **Statistics struct**: Performance metrics (events published/processed, queue size, etc.)
- **Global namespace**: `Global::getEventBus()`, `Global::setEventBus()`

**Key Methods:**
- `publish<EventType>(ProcessingMode, args...)` - Publish events with mode control
- `subscribe<EventType>(handler, name)` - Subscribe with automatic handle management
- `subscribeWithFilter<EventType>(handler, filter, name)` - Filtered subscription
- `subscribeWithPriority()`, `subscribeOnce()`, `subscribeFor()` - Specialized subscriptions
- `processDeferred(maxEvents)` - Process queued events
- `setGlobalFilter<EventType>()` - Global event filtering
- `getStatistics()` - Performance monitoring

### event_listeners.h  
**Inputs:**
- Depends on: `event_bus.h`, `event_types.h`, `../components/component.h`, `<flecs.h>`
- EventBus reference for subscription management
- Flecs entities for ECS integration
- Handler and filter function objects

**Outputs:**
- **ScopedEventListener<T>**: RAII event listener with automatic cleanup
- **EventListenerComponent<T>**: ECS component storing multiple event handles
- **ComponentEventListener<T>**: Entity-bound event listener with lifecycle management
- **ECSEventSystem**: System for ECS-integrated event handling with lifecycle observers
- **Lambda namespace**: Factory functions for lambda-based listeners
- **Utility classes**:
  - `MultiEventListener<EventTypes...>`: Multiple event type handling
  - `EntityEventHelper`: Fluent interface for entity event binding
  - Debug and performance monitoring utilities

**Key Methods:**
- `listen()`, `listenWithFilter()` - Basic event subscription
- `addListener()`, `addFilteredListener()` - Component-based subscription  
- `trackComponentChanges<T>()` - Automatic ECS lifecycle event publishing
- `createListener<EventType>(entity)` - Factory for entity-bound listeners
- `makeEventHelper(entity, eventBus)` - Fluent interface factory

## Data Flow

1. **Event Publishing**: 
   - Events published through `EventBus::publish<T>()` 
   - Processing mode determines immediate vs deferred handling
   - Events queued by priority and sequence ID for deterministic processing

2. **Event Processing**:
   - Immediate events: Direct dispatch to subscribers during publish call
   - Deferred events: Queued in priority queue, processed via `processDeferred()`
   - Global filters applied before listener-specific filters

3. **ECS Integration**:
   - `ECSEventSystem` automatically publishes entity/component lifecycle events
   - Component observers track add/remove/change events when enabled
   - Entity-bound listeners automatically cleaned up on entity destruction

4. **Thread Safety**:
   - Shared mutexes for read-heavy operations (listener lookup)
   - Atomic counters for ID generation and statistics
   - Optional thread safety disable for single-threaded performance

## Peripheral Dependencies

**Direct Dependencies:**
- `/ecs/components/component.h` - ECS component definitions (`KeyboardInput`, `MouseInput`, input state structs)
- `<flecs.h>` - Entity-component-system framework for observers and entity lifecycle
- `<SDL3/SDL.h>` - Input event structures and constants for SDL integration
- `<glm/glm.hpp>` - Vector/matrix types for spatial event data

**Integration Points:**
- `/ecs/systems/input_system.cpp` - Uses event types for SDL-to-ECS input conversion (currently limited usage)
- `/ecs/services/` - Service architecture could publish/subscribe to system events
- `/ecs/core/world_manager.h` - ECS world management for component lifecycle integration
- Future: Vulkan rendering system for frame/swapchain events
- Future: Physics system integration for collision/trigger events

**Service Architecture Compatibility:**
- Designed to integrate with ServiceLocator pattern for dependency injection
- Compatible with service lifecycle events (initialization, shutdown)
- Thread-safe design suitable for multi-service environments

## Key Notes

**Current State:**
- **Header-only implementation** - No .cpp files, templates and inline methods only
- **Not actively used** - Event system is implemented but not integrated into main application flow
- **Future-ready design** - Comprehensive event types defined for planned features

**Performance Characteristics:**
- Thread-safe with read-write lock optimization for listener access
- Priority queue for deterministic deferred event processing
- RAII handles prevent memory leaks and dangling subscriptions
- Statistics tracking for performance monitoring

**Integration Patterns:**
- ECS component lifecycle automatic event publishing via observers
- Entity-bound listeners with automatic cleanup on entity destruction
- Service-compatible design with dependency injection support
- Lambda-friendly interface for concise event handling

**Gotchas:**
- Template-heavy design may increase compilation time
- Deferred events require explicit `processDeferred()` calls in main loop
- Global event bus instance requires initialization before use
- Component change tracking must be explicitly enabled per component type

**Thread Safety:**
- Enabled by default, can be disabled for single-threaded performance
- All subscription/unsubscription operations are thread-safe
- Event publishing safe from multiple threads
- Deferred processing should occur on single thread (typically main thread)

---
**Update Note**: If any referenced components, services, or integration points change, this documentation requires updating to maintain accuracy.