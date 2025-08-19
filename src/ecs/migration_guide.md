# ECS Architecture Migration Guide

## Overview

This guide explains how to migrate from the basic ECS structure to the new AAA-grade modular architecture while maintaining backward compatibility.

## Migration Phases

### Phase 1: Service Registration (Immediate)

**Before (Old Pattern):**
```cpp
// Direct system access
InputManager::processSDLEvents(world);
CameraManager::updateAspectRatio(world, width, height);
```

**After (New Pattern):**
```cpp
// Service-based access
auto inputService = ServiceLocator::instance().getService<InputService>();
auto cameraService = ServiceLocator::instance().getService<CameraService>();

inputService->processInput();
cameraService->updateAspectRatio(width, height);
```

**Migration Steps:**
1. Register services in main.cpp:
```cpp
ServiceLocator::instance().createAndRegister<WorldManager>();
ServiceLocator::instance().createAndRegister<InputService>();
ServiceLocator::instance().createAndRegister<CameraService>();
ServiceLocator::instance().createAndRegister<RenderingService>();
```

2. Initialize services:
```cpp
auto worldManager = ServiceLocator::instance().getService<WorldManager>();
worldManager->initialize();

auto inputService = ServiceLocator::instance().getService<InputService>();
inputService->initialize(world, window);
```

### Phase 2: Module Encapsulation (Optional)

**Before (System-by-system):**
```cpp
// Register individual systems
world.system<Transform, MovementPattern>("movement_update")
    .each([](flecs::entity e, Transform& t, MovementPattern& p) {
        // Movement logic
    });
```

**After (Module-based):**
```cpp
// Load complete modules
auto inputModule = worldManager->loadModule<InputModule>("input");
auto movementModule = worldManager->loadModule<MovementModule>("movement", gpuEntityManager);
auto renderingModule = worldManager->loadModule<RenderingModule>("rendering", renderer, gpuEntityManager);
```

### Phase 3: Event System Integration (Advanced)

**Before (Direct coupling):**
```cpp
// Direct function calls between systems
if (inputPressed) {
    createEntity(mousePos);
    updateCamera();
}
```

**After (Event-driven):**
```cpp
// Event-based communication
EventBus::instance().subscribe<MouseClickEvent>([](const MouseClickEvent& event) {
    // Handle entity creation
});

EventBus::instance().publish(MouseClickEvent{mousePos, MouseButton::Left});
```

## Compatibility Matrix

| Feature | Old System | New System | Compatible |
|---------|------------|------------|------------|
| Component definitions | ✅ Same | ✅ Same | ✅ Yes |
| System registration | ✅ Works | ✅ Enhanced | ✅ Yes |
| GPU entity management | ✅ Works | ✅ Enhanced | ✅ Yes |
| Vulkan integration | ✅ Works | ✅ Bridged | ✅ Yes |
| Performance | ✅ High | ✅ Maintained | ✅ Yes |

## Step-by-Step Migration

### 1. Update main.cpp

```cpp
#include "ecs/core/world_manager.h"
#include "ecs/core/service_locator.h"
#include "ecs/services/input_service.h"
#include "ecs/services/camera_service.h"
#include "ecs/services/rendering_service.h"

int main() {
    // ... SDL and Vulkan setup ...
    
    // Initialize services
    auto worldManager = ServiceLocator::instance().createAndRegister<WorldManager>();
    auto inputService = ServiceLocator::instance().createAndRegister<InputService>();
    auto cameraService = ServiceLocator::instance().createAndRegister<CameraService>();
    auto renderingService = ServiceLocator::instance().createAndRegister<RenderingService>();
    
    // Initialize
    worldManager->initialize();
    inputService->initialize(worldManager->getWorld(), window);
    cameraService->initialize(worldManager->getWorld());
    renderingService->initialize(worldManager->getWorld(), &renderer);
    
    // Game loop
    while (running) {
        float deltaTime = calculateDeltaTime();
        
        // Use services
        inputService->processInput();
        
        // Execute ECS frame
        worldManager->executeFrame(deltaTime);
        
        // Render
        renderer.render();
    }
    
    // Cleanup
    ServiceLocator::instance().clear();
}
```

### 2. Gradual System Migration

**Option A: Keep existing systems (minimal change)**
```cpp
// Your existing systems continue to work
world.system<Transform, MovementPattern>("movement")
    .each([](flecs::entity e, Transform& t, MovementPattern& p) {
        // Existing code unchanged
    });
```

**Option B: Use service helpers (enhanced)**
```cpp
// Use new service-based systems
SystemHelpers::createServiceSystem<RenderingService>(world, "rendering_update")
    .inPhase(renderPhase)
    .build<Transform, Renderable>([](flecs::entity e, RenderingService& service, Transform& t, Renderable& r) {
        service.prepareEntity(e, t, r);
    });
```

### 3. Performance Monitoring Integration

```cpp
// Add performance monitoring
PerformanceTools::PerformanceMonitor monitor;
monitor.enable();
monitor.enableLogging("performance.csv");

// In game loop
monitor.startFrame();
worldManager->executeFrame(deltaTime);

// Periodic reporting
if (frameCount % 60 == 0) {
    monitor.printReport();
}
```

## API Reference Quick Guide

### Service Locator
```cpp
// Register service
ServiceLocator::instance().createAndRegister<ServiceType>(args...);

// Get service
auto service = ServiceLocator::instance().getService<ServiceType>();

// Require service (throws if not found)
auto& service = ServiceLocator::instance().requireService<ServiceType>();
```

### Module Management
```cpp
// Load module
auto module = worldManager->loadModule<ModuleType>("name", args...);

// Get module
auto module = worldManager->getModule<ModuleType>("name");

// Unload module
worldManager->unloadModule("name");
```

### Event System
```cpp
// Subscribe to events
auto handle = EventBus::instance().subscribe<EventType>([](const EventType& event) {
    // Handle event
});

// Publish events
EventBus::instance().publish(EventType{data});

// Scoped listener (auto-cleanup)
{
    ScopedEventListener listener = EventBus::instance().createScopedListener<EventType>(callback);
    // Automatically unsubscribes when scope ends
}
```

### Component Queries
```cpp
// Simple queries
auto renderables = ComponentQueries::CommonQueries::renderableEntities(world);
renderables.each([](flecs::entity e, Transform& t, Renderable& r) {
    // Process entities
});

// Filtered queries
auto nearbyEntities = ComponentQueries::Filters::byPosition(center, radius)
    .apply(renderables);
```

### Performance Tools
```cpp
// System profiling
PerformanceTools::PerformanceMonitor monitor;
{
    PROFILE_SCOPE(monitor, "system_name");
    // System code here
}

// Frame timing
PerformanceTools::FrameTimer frameTimer;
float deltaTime = frameTimer.tick();
std::cout << "FPS: " << frameTimer.getFPS() << std::endl;
```

## Performance Notes

- **Zero Overhead**: Service locator uses templates for compile-time resolution
- **Cache Friendly**: Maintains existing component layout and access patterns
- **Minimal Allocations**: RAII and move semantics throughout
- **Backward Compatible**: Existing systems continue to work without changes

## Best Practices

1. **Start Small**: Begin with service registration, then add modules gradually
2. **Profile Early**: Use performance tools to ensure no regression
3. **Event Sparingly**: Don't over-engineer with events for high-frequency operations
4. **Module Boundaries**: Keep modules focused on single responsibilities
5. **Service Dependencies**: Use service locator for loose coupling

The migration can be done incrementally over multiple development cycles while maintaining full functionality at each step.