# ECS Input Services

## Overview
Action-based input system providing configurable key bindings, context switching, and seamless ECS integration for the Fractalia2 engine.

## Architecture

### Core Pattern: Action → Binding → Context
```
Physical Input (SDL) → InputBinding → InputAction → ECS Components
                         ↓               ↓            ↓
                    Raw events      Logical actions  Game state
```

### Key Classes

**InputActionSystem** - Core action management
- Maps physical inputs to logical actions (move, jump, shoot)
- Supports digital (on/off), analog 1D (triggers), analog 2D (mouse/stick)
- Action queries: `isActionJustPressed()`, `getActionAnalog2D()`
- Callback registration for immediate response

**InputContextManager** - Context switching with priority
- Groups bindings by context (menu, gameplay, debug)
- Priority-based resolution (higher priority overrides lower)
- Context stack for temporary mode switching
- Example: Menu context blocks gameplay actions

**InputEventProcessor** - SDL event handling
- Raw keyboard/mouse state tracking
- Frame-based pressed/released detection
- Window event processing (resize, quit)
- Input consumption for UI systems

**InputConfigManager** - Configuration persistence
- Load/save key bindings from files
- Default action/context setup
- Runtime binding modification

**InputECSBridge** - ECS integration
- Synchronizes input state to ECS components
- World coordinate conversion for mouse
- Frame tracking for consistent updates

### Data Flow

1. **Event Processing**: SDL events → `InputEventProcessor` → Raw state
2. **Action Evaluation**: Raw state + Context bindings → `InputActionSystem` → Action states
3. **ECS Sync**: Action states → `InputECSBridge` → ECS components
4. **Game Logic**: ECS systems query components or use action callbacks

### Input Types & Binding

```cpp
// Action types
InputActionType::DIGITAL     // Keys, buttons (true/false)
InputActionType::ANALOG_1D   // Mouse wheel, triggers (float)
InputActionType::ANALOG_2D   // Mouse position, sticks (vec2)

// Binding structure
InputBinding {
    InputType inputType;     // KEYBOARD_KEY, MOUSE_BUTTON, etc.
    int keycode/mouseButton; // Physical input identifier
    float sensitivity;       // Analog scaling
    bool requiresShift/Ctrl; // Modifier requirements
}
```

### Context System

**Priority Resolution**: Higher priority contexts override lower ones
- Menu (100) > Debug (50) > Gameplay (0)
- Action bindings resolved in priority order

**Context Stack**: Temporary activation
```cpp
contextManager.pushContext("debug");    // Activate debug mode
// ... debug actions available
contextManager.popContext();           // Return to previous
```

### Essential Usage Patterns

**Action Registration**:
```cpp
InputActionDefinition moveAction {
    .name = "move_forward",
    .type = InputActionType::DIGITAL,
    .defaultBindings = { InputBinding(KEYBOARD_KEY, SDLK_W) }
};
actionSystem.registerAction(moveAction);
```

**Action Queries**:
```cpp
if (actionSystem.isActionJustPressed("jump")) { /* handle jump */ }
glm::vec2 moveDir = actionSystem.getActionAnalog2D("mouse_look");
```

**Context Management**:
```cpp
// Setup contexts with priority
contextManager.registerContext("gameplay", 0);
contextManager.registerContext("menu", 100);

// Bind actions to contexts
contextManager.bindAction("gameplay", "move_forward", keyBinding);
contextManager.setContextActive("menu", true);  // Activates menu, blocks gameplay
```

## Integration Points

- **Service Pattern**: Integrates with ServiceLocator for dependency injection
- **ECS Bridge**: Synchronizes to ECS components for system queries
- **Camera Service**: Mouse world coordinate conversion
- **UI Systems**: Input consumption to prevent game action bleeding

## Performance Notes

- Frame-based state tracking prevents missed inputs
- Context resolution cached per frame
- Action callbacks for immediate response without polling
- Minimal allocation during runtime (setup-time only)