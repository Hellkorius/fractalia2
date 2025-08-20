# Input Services Directory - AI Agent Context

## Purpose
Modular input subsystem providing action-based input management, SDL event processing, context switching, and ECS integration for the Fractalia2 engine. Contains specialized components orchestrated by parent InputService.

## File/Folder Hierarchy
```
src/ecs/services/input/
├── CLAUDE.md                      # This file - AI agent context
├── Input.md                       # Detailed system documentation
├── input_types.h                  # Core data structures and enums
├── input_action_system.h/cpp      # Action binding, mapping, callbacks
├── input_event_processor.h/cpp    # SDL event handling, raw input state  
├── input_context_manager.h/cpp    # Context switching, priority, stack management
├── input_config_manager.h/cpp     # Configuration loading/saving, defaults
└── input_ecs_bridge.h/cpp         # ECS component synchronization

Parent Integration:
../input_service.h/cpp             # Orchestration layer, service interface
```

## Core Data Flow Architecture

### Processing Pipeline (Per Frame):
```
SDL Events → InputEventProcessor → KeyboardState/MouseState
                ↓
InputContextManager → Active context bindings
                ↓  
InputActionSystem ← (raw states + context bindings) → InputActionState
                ↓
InputECSBridge ← (processed input data) → ECS Components
```

### Module Dependencies:
- **InputEventProcessor**: No dependencies (foundation layer)
- **InputContextManager**: No dependencies (context definitions)
- **InputConfigManager**: Reads from InputContextManager, writes to InputActionSystem
- **InputActionSystem**: Reads from InputEventProcessor + InputContextManager
- **InputECSBridge**: Reads from InputEventProcessor + InputActionSystem

## Input/Output Specifications

### InputEventProcessor
**Consumes:** SDL_Event structs, SDL_Window* reference
**Produces:** KeyboardState/MouseState structs, window events (resize, quit)
**API Surface:** Raw input queries (isKeyDown, getMousePosition, etc.)

### InputActionSystem  
**Consumes:** KeyboardState/MouseState, active context bindings, InputActionDefinition
**Produces:** InputActionState structs, callback execution results
**API Surface:** Action queries (isActionActive, getActionAnalog1D/2D), callback registration

### InputContextManager
**Consumes:** InputContextDefinition, context activation requests
**Produces:** Active context priority resolution, filtered binding lists
**API Surface:** Context management (setContextActive, pushContext, getCurrentContext)

### InputConfigManager
**Consumes:** Config files (JSON), default definitions
**Produces:** Loaded InputActionDefinition/InputContextDefinition, saved config files
**API Surface:** Config operations (loadInputConfig, saveInputConfig, resetToDefaults)

### InputECSBridge
**Consumes:** KeyboardState/MouseState, flecs::world reference
**Produces:** KeyboardInput/MouseInput/InputState/InputEvents ECS components
**API Surface:** ECS synchronization (synchronizeToECSComponents), component access

## Key Data Structures

### Core Types (input_types.h):
```cpp
enum class InputActionType { DIGITAL, ANALOG_1D, ANALOG_2D };
struct InputBinding { InputType, keycode/mouseButton, modifiers, sensitivity };
struct InputActionState { type, digitalValue, analogValue1D/2D, justPressed/Released, duration };
struct InputActionDefinition { name, type, description, defaultBindings };
```

### Internal State:
```cpp
struct KeyboardState { keys[512], keysPressed[512], keysReleased[512], modifiers };
struct MouseState { buttons[8], position, delta, wheelDelta };
```

## Integration Points

### Parent InputService Orchestration:
- **Delegation Pattern**: InputService.h delegates all API calls to appropriate modules
- **Initialization Order**: EventProcessor → ContextManager → ConfigManager → ActionSystem → ECSBridge
- **Service Dependencies**: CameraService* (cached), flecs::world&, SDL_Window*

### External Dependencies:
- **SDL3**: Event processing, window management
- **Flecs ECS**: Component synchronization, entity management  
- **GLM**: Vector math for mouse coordinates and analog inputs
- **CameraService**: World coordinate transformations for mouse position

### ECS Component Bridge:
- Creates single input entity with KeyboardInput, MouseInput, InputState, InputEvents components
- Synchronizes raw input state to ECS components each frame
- Provides component access for other ECS systems

## Performance Characteristics
- **Zero-allocation input processing**: Uses fixed-size arrays for raw input state
- **Efficient binding evaluation**: Early termination for inactive contexts
- **Frame-coherent updates**: All modules process once per frame in sequence
- **Cached service references**: Avoids repeated ServiceLocator lookups

## Integration Patterns

### Service Access Pattern:
```cpp
// From parent InputService - delegates to modules
inputService.isActionJustPressed("jump") → actionSystem->isActionJustPressed("jump")
inputService.getMousePosition() → eventProcessor->getMousePosition()
```

### Module Communication:
```cpp
// ActionSystem queries other modules for current state
actionSystem.updateActionStates(eventProcessor.getKeyboardState(), 
                               eventProcessor.getMouseState(),
                               contextManager, deltaTime);
```

### ECS Integration:
```cpp
// ECSBridge synchronizes processed data to components
ecsBridge.synchronizeToECSComponents(keyboardState, mouseState, deltaTime);
// Other systems access via component queries
```

## Key Implementation Notes
- **Context Priority**: Higher priority contexts override lower priority bindings
- **Modifier Support**: Shift/Ctrl/Alt requirements checked per binding
- **Analog Processing**: Deadzone, sensitivity, and axis inversion applied per binding
- **Callback System**: Action callbacks executed after state updates each frame
- **Input Consumption**: UI systems can mark input as consumed to prevent game processing

---
**⚠️ Update Requirement**: This file must be updated when:
- Module APIs change (input/output contracts)
- New modules added or removed  
- Integration patterns with InputService change
- ECS component structure modifications
- Parent service dependencies change
- Data flow architecture modifications