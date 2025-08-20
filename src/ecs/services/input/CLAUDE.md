# Input Service Modules - AI Agent Context

## Purpose
Modularized input subsystems providing action-based input management, SDL event processing, context switching, and ECS integration. Each module handles a specific aspect of the input system for clean separation of concerns.

## File/Folder Hierarchy
```
src/ecs/services/input/
├── input_action_system.h/cpp      # Action binding, mapping, callbacks
├── input_event_processor.h/cpp    # SDL event handling, raw input state  
├── input_context_manager.h/cpp    # Context switching, priority, stack management
├── input_config_manager.h/cpp     # Configuration loading/saving, defaults
└── input_ecs_bridge.h/cpp         # ECS component synchronization
```

## Inputs & Outputs (by Module)

### InputActionSystem
**Inputs:**
- InputActionDefinition structs (action name, type, bindings)
- InputBinding structs (key mappings, modifiers, sensitivity)
- Input callbacks (std::function<void(const std::string&, const InputActionState&)>)
- Raw input state from InputEventProcessor
- Active context bindings from InputContextManager

**Outputs:**
- InputActionState structs (digital/analog values, just pressed/released)
- Action query results (bool, float, glm::vec2)
- Callback execution for active actions
- Action duration tracking

**Key APIs:**
- `registerAction()` - add new action definition
- `bindAction()`, `unbindAction()` - modify bindings
- `isActionActive()`, `isActionJustPressed()` → bool
- `getActionAnalog1D/2D()` → float/glm::vec2
- `registerActionCallback()` - add callback for action

### InputEventProcessor
**Inputs:**
- SDL_Event structs from SDL event queue
- SDL_Window* for relative input queries
- Delta time for input state timing

**Outputs:**
- KeyboardState struct (key arrays, modifier flags)
- MouseState struct (button arrays, position, deltas)
- Window event state (resize, quit flags)
- Raw input query results

**Key APIs:**
- `processSDLEvents()` - consume SDL event queue
- `isKeyDown()`, `isKeyPressed()`, `isKeyReleased()` → bool
- `isMouseButtonDown()`, `getMousePosition()` → bool/glm::vec2
- `getMouseDelta()`, `getMouseWheelDelta()` → glm::vec2
- `hasWindowResizeEvent()` → bool + dimensions

### InputContextManager
**Inputs:**
- InputContextDefinition structs (name, bindings, priority)
- Context activation/deactivation requests
- Context stack push/pop operations

**Outputs:**
- Active context name and priority
- Context stack state
- Action binding resolution for active contexts
- Context-filtered binding lists

**Key APIs:**
- `registerContext()` - add new input context
- `setContextActive()` - activate/deactivate context
- `pushContext()`, `popContext()` - temporary context stack
- `getCurrentContext()` → string
- `getActiveContextBindings()` → bindings for action

### InputConfigManager
**Inputs:**
- Configuration file paths (JSON/custom format)
- Default action/binding definitions
- User customization requests

**Outputs:**
- Loaded InputActionDefinition and InputContextDefinition structs
- Saved configuration files
- Restored default bindings

**Key APIs:**
- `loadInputConfig()` - load from file
- `saveInputConfig()` - save current state
- `resetToDefaults()` - restore default bindings
- `createDefaultActions()`, `createDefaultContexts()` - setup defaults

### InputECSBridge
**Inputs:**
- flecs::world& - ECS world reference
- Input entity handle
- Internal input state (keyboard, mouse, actions)

**Outputs:**
- KeyboardInput, MouseInput, InputState, InputEvents components
- ECS entity with synchronized input data
- Component updates per frame

**Key APIs:**
- `synchronizeToECSComponents()` - update ECS components
- `getKeyboardInput()`, `getMouseInput()` → component pointers
- `getInputState()`, `getInputEvents()` → component pointers

## Data Flow Between Modules

### Frame Processing Flow:
```
InputEventProcessor.processSDLEvents()
  → Updates raw keyboard/mouse state
  → Handles window events

InputActionSystem.updateActionStates()
  → Reads raw input state from EventProcessor
  → Reads active bindings from ContextManager
  → Updates action states and executes callbacks

InputECSBridge.synchronizeToECSComponents()
  → Reads processed input data from all modules
  → Updates ECS components for other systems
```

### Module Interaction Pattern:
- **InputService** orchestrates all modules, manages lifecycle
- **ActionSystem** queries EventProcessor for raw input, ContextManager for bindings
- **ContextManager** provides binding resolution for ActionSystem
- **ConfigManager** initializes ActionSystem and ContextManager defaults
- **ECSBridge** reads state from ActionSystem and EventProcessor

### Initialization Order:
1. EventProcessor (raw input handling)
2. ContextManager (context definitions)
3. ConfigManager (load defaults and user config)
4. ActionSystem (action definitions and bindings)
5. ECSBridge (ECS integration setup)

## Key Types and Structures

### Core Input Types:
```cpp
enum class InputActionType { DIGITAL, ANALOG_1D, ANALOG_2D };

struct InputActionState {
    InputActionType type;
    bool digitalValue;
    float analogValue1D;
    glm::vec2 analogValue2D;
    bool justPressed, justReleased;
    float duration;
};

struct InputBinding {
    enum class InputType { KEYBOARD_KEY, MOUSE_BUTTON, MOUSE_AXIS_X, ... };
    InputType inputType;
    union { int keycode; int mouseButton; int axisIndex; };
    bool requiresShift, requiresCtrl, requiresAlt;
    float deadzone, sensitivity;
    bool invertAxis;
};
```

### Context Management:
```cpp
struct InputContextDefinition {
    std::string name;
    std::unordered_map<std::string, std::vector<InputBinding>> actionBindings;
    int priority;
    bool active;
};
```

### Internal State:
```cpp
struct KeyboardState {
    static constexpr size_t MAX_KEYS = 512;
    bool keys[MAX_KEYS], keysPressed[MAX_KEYS], keysReleased[MAX_KEYS];
    bool shift, ctrl, alt;
};

struct MouseState {
    static constexpr size_t MAX_BUTTONS = 8;
    bool buttons[MAX_BUTTONS], buttonsPressed[MAX_BUTTONS], buttonsReleased[MAX_BUTTONS];
    glm::vec2 position, delta, wheelDelta;
};
```

## Integration with Parent InputService

### InputService Orchestration:
- **initialize()**: Initializes all modules in correct order
- **processFrame()**: Calls module update methods in sequence
- **cleanup()**: Shuts down modules in reverse order
- **Service Interface**: Delegates API calls to appropriate modules

### Service Dependencies:
- CameraService* (cached) - for world coordinate transformations
- flecs::world& - ECS integration
- SDL_Window* - SDL event processing

### Performance Considerations:
- Direct state arrays for raw input (no allocations)
- Cached service references to avoid repeated lookups
- Frame-coherent processing (all modules update once per frame)
- Efficient binding evaluation with early termination

---
**⚠️ Update Requirement**: This file must be updated whenever:
- New input modules are added
- Module APIs change (inputs/outputs)
- Dependencies between modules change
- Integration patterns with InputService change
- ECS component structures change