# Input Services Module

(Modular input system providing 3D camera controls with action-based input management, SDL event processing, context switching, and ECS integration)

## Folder Structure

```
input/
├── input_types.h
├── input_action_system.h/cpp
├── input_event_processor.h/cpp
├── input_context_manager.h/cpp
├── input_config_manager.h/cpp
├── input_ecs_bridge.h/cpp
└── Input.md
```

## Files

### input_types.h
**Inputs:** None (type definitions only)
**Outputs:** InputActionType enum (DIGITAL, ANALOG_1D, ANALOG_2D), InputBinding struct with modifiers/sensitivity/deadzone supporting KEYBOARD_KEY, MOUSE_BUTTON, MOUSE_AXIS_X/Y, MOUSE_WHEEL_X/Y input types, InputActionDefinition with name/type/default bindings, InputActionState with digital/analog values and timing. Provides core type system for 3D input action mapping including mouse look and analog controls.

### input_action_system.h
**Inputs:** InputActionDefinition registration, KeyboardState/MouseState from event processor, InputContextManager for binding resolution, action callbacks
**Outputs:** InputActionState updates with digital/analog values, action query results (active/pressed/released states), callback execution for registered actions. Manages action-to-input binding evaluation and provides high-level action queries.

### input_action_system.cpp
**Inputs:** Raw keyboard/mouse state, active context bindings, deltaTime for duration tracking
**Outputs:** Updated action states with timing information, executed callbacks for state changes, binding evaluation results. Implements core action system logic including modifier checking and analog value processing.

### input_event_processor.h
**Inputs:** SDL_Event queue, SDL_Window reference for coordinate systems
**Outputs:** KeyboardState arrays with pressed/released tracking, MouseState with position/delta/wheel, window event flags (resize/quit). Provides raw SDL input processing and state maintenance.

### input_event_processor.cpp
**Inputs:** SDL event polling, keyboard scancode mappings, mouse button/motion/wheel events
**Outputs:** Frame-coherent keyboard/mouse state arrays, modifier key tracking, window event consumption. Handles SDL event loop processing and maintains raw input state buffers.

### input_context_manager.h
**Inputs:** InputContextDefinition registration, context activation/deactivation requests, priority-based context stack operations
**Outputs:** Active context resolution, binding priority ordering, context stack state management. Manages input context switching and binding resolution with priority systems.

### input_context_manager.cpp
**Inputs:** Context definitions with priority levels, push/pop context operations, action binding queries
**Outputs:** Priority-sorted active contexts, filtered binding lists per action, context stack maintenance. Implements context priority resolution and binding conflict management.

### input_config_manager.h
**Inputs:** Configuration file paths, default action/context definitions, reset-to-defaults requests
**Outputs:** Loaded InputActionDefinition and InputContextDefinition configurations, saved configuration persistence, default binding restoration. Manages input configuration loading/saving and default setup.

### input_config_manager.cpp
**Inputs:** JSON/config file parsing, InputActionSystem and InputContextManager references for configuration
**Outputs:** Registered 3D camera movement actions (forward W, backward S, strafe left A, strafe right D, up Space, down Shift), mouse look (MOUSE_AXIS_X/Y), zoom (MOUSE_WHEEL_Y), mouse button actions, default contexts (default, gameplay, ui, debug), configuration file persistence. Implements 3D navigation input scheme with WASD+Space/Shift movement and mouse controls.

### input_ecs_bridge.h
**Inputs:** flecs::world reference, input entity creation, processed input states from other modules
**Outputs:** ECS component synchronization (KeyboardInput, MouseInput, InputState, InputEvents), world coordinate transformations with CameraService integration. Bridges input system data to ECS components.

### input_ecs_bridge.cpp
**Inputs:** KeyboardState, MouseState from event processor, action states from action system, camera service for world coordinate conversion
**Outputs:** Updated ECS input components per frame, mouse world position calculations, synchronized input entity state. Implements ECS integration layer for input data access.

### Input.md
**Inputs:** None (documentation file)
**Outputs:** Comprehensive module documentation including data flow diagrams, API reference, initialization order, and integration patterns. Serves as detailed technical reference for the input service architecture.

## 3D Camera Input System

The input system provides comprehensive 3D camera controls with action-based input mapping:

### 3D Movement Actions
- **camera_move_forward** (W) - Move camera forward along view direction
- **camera_move_backward** (S) - Move camera backward along view direction  
- **camera_move_left** (A) - Strafe camera left perpendicular to view direction
- **camera_move_right** (D) - Strafe camera right perpendicular to view direction
- **camera_move_up** (Space) - Move camera up along world Y-axis
- **camera_move_down** (Left Shift) - Move camera down along world Y-axis

### Mouse Look Controls
- **mouse_look** (MOUSE_AXIS_X/Y) - 3D camera rotation with horizontal/vertical mouse movement
- **camera_toggle_mouse_look** - Enable/disable mouse look mode for camera rotation

### Camera Zoom/FOV
- **camera_zoom** (MOUSE_WHEEL_Y) - FOV adjustment for perspective cameras, zoom for orthographic cameras

### Input Features
- **Analog Inputs**: Mouse axis movement (MOUSE_AXIS_X/Y) and mouse wheel (MOUSE_WHEEL_Y) 
- **Digital Actions**: Keyboard keys (WASD, Space, Shift) and mouse buttons
- **Context Management**: Default, gameplay, ui, and debug input contexts with priority-based resolution
- **Modifier Support**: Shift, Ctrl, Alt key modifiers with sensitivity and deadzone settings
- **Action Callbacks**: Registerable callbacks for custom action handling

### Default 3D Navigation Context
The default input context provides immediate 3D camera navigation:
- WASD keys for forward/backward/strafe movement
- Space/Shift for vertical movement (up/down)
- Mouse movement for camera look (when mouse look enabled)
- Mouse wheel for FOV/zoom adjustment
- Mouse buttons for primary/secondary actions