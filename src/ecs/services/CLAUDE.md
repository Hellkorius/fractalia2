# ECS Services Directory

**camera/** (Camera system management and viewport handling)
- **camera.md** - Documentation for camera system architecture and features
- **camera_culling.cpp** - Consumes camera bounds and entity transforms. Produces visibility flags for entities within camera frustum
- **camera_culling.h** - Defines camera culling interface and visibility testing functions
- **camera_manager.cpp** - Consumes camera creation requests and ECS world. Produces camera entities with proper component initialization and lifecycle management
- **camera_manager.h** - Defines camera management interface with ID-based camera access and entity mapping
- **camera_transforms.cpp** - Consumes camera position/rotation/zoom parameters. Produces view and projection matrices for rendering pipeline
- **camera_transforms.h** - Defines camera transformation calculations and matrix generation interface
- **camera_transition_system.cpp** - Consumes transition requests and timing data. Produces smooth camera interpolation between states over time
- **camera_transition_system.h** - Defines camera transition interface with easing functions and state management
- **viewport_manager.cpp** - Consumes viewport definitions and camera associations. Produces viewport configurations for multi-camera rendering
- **viewport_manager.h** - Defines viewport management interface with name-based viewport access and activation

**input/** (Input processing and action mapping subsystem)
- **Input.md** - Documentation for input system architecture and action mapping
- **input_action_system.cpp** - Consumes raw input states and context bindings. Produces action states with analog values and callback execution
- **input_action_system.h** - Defines action system interface with binding evaluation and callback management
- **input_config_manager.cpp** - Consumes configuration files and binding definitions. Produces saved/loaded input configurations with validation
- **input_config_manager.h** - Defines configuration management interface for persistent input settings
- **input_context_manager.cpp** - Consumes context registration and activation requests. Produces prioritized context stack for input resolution
- **input_context_manager.h** - Defines context management interface with priority-based activation system
- **input_ecs_bridge.cpp** - Consumes ECS world and input states. Produces ECS components synchronized with input system state
- **input_ecs_bridge.h** - Defines ECS integration interface for input component management
- **input_event_processor.cpp** - Consumes SDL events and window context. Produces processed keyboard/mouse states with event tracking
- **input_event_processor.h** - Defines event processing interface with raw input state management
- **input_types.h** - Defines input system data structures including bindings, actions, and state representations

**camera_service.cpp** - Consumes camera requests, viewport definitions, and frame updates. Produces unified camera management with transitions, culling, and coordinate transformations

**camera_service.h** - Defines comprehensive camera service interface integrating all camera subsystems with ECS world

**control_service.cpp** - Consumes input actions, camera service, and rendering service. Produces game control logic with entity creation, debug commands, and performance monitoring

**control_service.h** - Defines control service interface with action registration, state management, and service coordination

**input_service.cpp** - Consumes SDL events, window context, and action definitions. Produces comprehensive input management with context switching and callback execution

**input_service.h** - Defines input service interface integrating all input subsystems with action-based input handling

**rendering_service.cpp** - Consumes ECS entities with renderable components and camera data. Produces render queue with culling, batching, and GPU synchronization

**rendering_service.h** - Defines rendering service interface with render queue management, statistics tracking, and GPU pipeline coordination