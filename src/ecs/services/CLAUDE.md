# ECS Services - Core Game Systems

## Purpose
High-level service-based architecture for game functionality coordination. Provides dependency-injected services that bridge ECS components with external systems (SDL, Vulkan) using priority-based initialization.

## Architecture Pattern
- **Service Locator**: `DECLARE_SERVICE()` macro for standardized interface
- **Priority Initialization**: Services initialize in dependency order (100→60)
- **Dependency Injection**: Services access each other through ServiceLocator
- **Frame Coordination**: Single-threaded frame processing with proper ordering

## Core Services

### CameraService (Priority: 80)
**Purpose**: Multi-camera management with smooth transitions and viewport support
```cpp
// Key APIs
CameraID createCamera(const std::string& name = "");
void transitionToCamera(CameraID target, CameraTransitionType type, float duration);
std::vector<CullingInfo> performFrustumCulling(const std::vector<Transform>& transforms, const std::vector<Bounds>& bounds);
glm::vec2 worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize);
```
**Modules**: camera_manager, camera_transitions, viewport_manager, camera_culling, camera_transforms

### InputService (Priority: 90)
**Purpose**: Action-based input system with context switching
```cpp
// Key APIs
bool isActionJustPressed(const std::string& actionName);
glm::vec2 getActionAnalog2D(const std::string& actionName);
void setContextActive(const std::string& contextName, bool active);
glm::vec2 getMouseWorldPosition();
```
**Modules**: input_action_system, input_event_processor, input_context_manager, input_config_manager, input_ecs_bridge

### RenderingService (Priority: 70)
**Purpose**: ECS-Vulkan bridge with render queue management and culling optimization
```cpp
// Key APIs
void buildRenderQueue();
void performCulling();
void createRenderBatches();
void submitRenderQueue();
const CullingStats& getCullingStats();
```
**Features**: LOD system, frustum culling, render batching, GPU-driven pipeline

### GameControlService (Priority: 60)
**Purpose**: Game control logic and inter-service coordination
```cpp
// Key APIs
void processFrame(float deltaTime);
void createEntity(const glm::vec2& position);
void createSwarm(size_t count, const glm::vec3& center, float radius);
void toggleMovementType();
```
**Integration**: Coordinates input → camera → rendering pipeline

## Data Flow
```
Frame Processing:
GameControlService.processFrame()
  → InputService.processFrame()    // SDL events → actions
  → CameraService.update()         // Transitions, active camera
  → RenderingService.processFrame()
    → buildRenderQueue()           // ECS entities
    → performCulling()             // CameraService culling
    → createRenderBatches()        // GPU optimization
    → submitRenderQueue()          // VulkanRenderer

Inter-Service Communication:
Input → Camera: Mouse world coords, viewport queries
Camera → Rendering: View matrices, culling results
Rendering → GPU: Entity data, transform buffers
Control → All: Commands, state changes, debug toggles
```

## Key Interfaces

### Service Pattern
```cpp
class MyService {
    DECLARE_SERVICE(MyService);
    bool initialize(flecs::world& world, ...);
    void cleanup();
    void processFrame(float deltaTime);
};
```

### Dependency Access
```cpp
// Initialize dependencies
cameraService = &ServiceLocator::instance().requireService<CameraService>();

// Or use convenience macro
SERVICE(InputService).isActionJustPressed("jump");
```

## Integration Points
- **ECS World**: All services operate on shared `flecs::world&`
- **SDL Integration**: InputService processes SDL events
- **Vulkan Pipeline**: RenderingService submits to VulkanRenderer
- **GPU Bridge**: Services coordinate through GPUEntityManager
- **Service Locator**: Centralized dependency management with lifecycle tracking

## Performance Considerations
- **Cached Service References**: Avoid repeated ServiceLocator lookups
- **Batch Processing**: Camera culling operates on entity arrays  
- **GPU Optimization**: Render service minimizes draw calls through batching
- **Early Termination**: Skip rendering when no entities visible
- **Thread Safety**: Service access protected by mutex, frame processing single-threaded