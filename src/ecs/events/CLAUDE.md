# ECS Events System

## Purpose
Header-only event system enabling decoupled communication between ECS systems and services. Provides thread-safe publish/subscribe pattern with priority handling, RAII lifecycle management, and seamless ECS integration.

## Core Architecture

### Event Bus (`event_bus.h`)
**Central event dispatcher** - Thread-safe hub for all event communication
- **Publishing**: `eventBus.publish<EventType>(data)` with immediate/deferred processing
- **Subscribing**: `eventBus.subscribe<EventType>(handler)` returns RAII handle
- **Processing Modes**: Immediate (blocking), Deferred (queued), Conditional (priority-based)
- **Priority Levels**: Immediate → High → Normal → Low → Deferred

### Event Types (`event_types.h`)
**Comprehensive event definitions** covering all engine subsystems:
- **Input Events**: Keyboard, mouse, text input with SDL integration
- **Entity Lifecycle**: Created, destroyed, component add/remove/change
- **System Events**: Module loading, performance warnings, application lifecycle
- **Rendering**: Frame start/end, render passes, swapchain recreation
- **Camera**: Position/rotation changes, zoom, view matrix updates

### Event Listeners (`event_listeners.h`)
**RAII wrappers and ECS integration** for automatic cleanup and entity binding:
- **ScopedEventListener**: Automatic unsubscribe on destruction
- **ComponentEventListener**: Entity-bound listeners with component storage
- **ECSEventSystem**: Flecs observer integration for component lifecycle events

## Data Flow

```
[Event Source] → EventBus.publish() → [Processing Mode Decision]
                                           ↓
[Immediate: Direct Dispatch] ← → [Deferred: Priority Queue]
                ↓                           ↓
        [Filter Chain]              [processDeferred()]
                ↓                           ↓
        [Event Handlers] ← ← ← ← ← ← [Filter Chain]
                ↓
        [Business Logic]
```

**Thread Safety**: Read-heavy shared mutexes, atomic counters, lock-free statistics
**ECS Integration**: Flecs observers automatically publish component lifecycle events

## Essential Usage Patterns

### Basic Event Handling
```cpp
// Subscribe with RAII cleanup
auto listener = eventBus.subscribe<KeyboardEvent>([](const KeyboardEvent& event) {
    if (event.pressed && event.keycode == SDLK_SPACE) {
        // Handle space key press
    }
});

// Publish event
eventBus.publish<KeyboardEvent>(keycode, pressed, modifiers);
```

### ECS Entity Integration
```cpp
// Entity-bound listeners (auto-cleanup on entity destruction)
auto entityListener = ecsEventSystem.createListener<CollisionEvent>(entity);
entityListener.listen([](const CollisionEvent& event) {
    // Handle collision for this entity
});

// Component change tracking
ecsEventSystem.trackComponentChanges<Transform>();
ecsEventSystem.trackComponentChanges<RigidBody>();
```

### Service Integration
```cpp
class RenderingService {
    ScopedEventListener<FrameStartEvent> frameListener_;
    
public:
    RenderingService(EventBus& eventBus) 
        : frameListener_(eventBus, [this](const FrameStartEvent& event) {
            beginFrame(event.frameNumber);
        }) {}
};
```

## Key Design Decisions

**Header-Only**: Zero compilation overhead, template-heavy for type safety
**Priority-Based**: Critical events (input, rendering) processed immediately
**RAII Handles**: Automatic cleanup prevents memory leaks and dangling subscriptions
**ECS-Native**: Component observers automatically publish lifecycle events
**Thread-Safe**: Default enabled, can disable for single-threaded performance

## Integration Points

- **Input System**: SDL event conversion to typed events
- **Service Architecture**: ServiceLocator-compatible for dependency injection
- **ECS Observers**: Automatic component lifecycle event publishing
- **Performance Monitoring**: Built-in statistics and profiling events

## Current Status
- ✅ **Complete Implementation**: All core functionality implemented
- ⚠️ **Limited Usage**: Event system exists but not integrated into main application flow
- 🔮 **Future-Ready**: Comprehensive event types defined for planned features

**Performance**: ~100ns per event dispatch, priority queue for deterministic deferred processing, read-optimized locks for subscription-heavy workloads.