# ECS Services Directory

**camera/** (3D camera system management and viewport handling)
- **camera.md** - Documentation for camera system architecture and features
- **camera_culling.cpp** - 3D frustum culling and visibility testing using perspective projection
- **camera_culling.h** - Defines 3D camera culling interface with frustum testing functions
- **camera_manager.cpp** - Camera entity management with 3D perspective and orthographic modes
- **camera_manager.h** - Camera management interface with 3D position/target/up vectors
- **camera_transforms.cpp** - 3D view/projection matrix calculations for perspective cameras
- **camera_transforms.h** - 3D transformation interface with lookAt and perspective matrices
- **camera_transition_system.cpp** - Smooth camera transitions between 3D positions and orientations
- **camera_transition_system.h** - Camera transition interface with 3D interpolation support
- **viewport_manager.cpp** - Multi-viewport configuration for 3D camera rendering
- **viewport_manager.h** - Viewport management interface supporting 3D camera associations

**input/** (Input processing with 3D camera action mapping)
- **Input.md** - Documentation for input system architecture and 3D camera actions
- **input_action_system.cpp** - Action system supporting 3D camera movement and mouse look
- **input_action_system.h** - Action interface with analog inputs for camera rotation
- **input_config_manager.cpp** - Configuration system with 3D camera control defaults
- **input_config_manager.h** - Configuration management for persistent camera settings
- **input_context_manager.cpp** - Context management supporting camera control contexts
- **input_context_manager.h** - Priority-based context system for input resolution
- **input_ecs_bridge.cpp** - ECS integration for 3D camera input components
- **input_ecs_bridge.h** - ECS bridge interface for camera input synchronization
- **input_event_processor.cpp** - SDL event processing including mouse motion for camera look
- **input_event_processor.h** - Event processor interface with mouse delta tracking
- **input_types.h** - Input data structures supporting analog camera controls

**camera_service.cpp** - 3D camera management with perspective projection, FOV controls, and lookAt functionality

**camera_service.h** - Comprehensive 3D camera service interface with perspective/orthographic support

**control_service.cpp** - 3D camera controls (WASD movement, mouse look, FOV zoom) integrated with game logic

**control_service.h** - Control service interface coordinating 3D camera movement and mouse look

**input_service.cpp** - Input management supporting 3D camera actions (movement, rotation, FOV)

**input_service.h** - Input service interface with 3D camera control action definitions

**rendering_service.cpp** - Render queue management with 3D camera frustum culling

**rendering_service.h** - Rendering service interface with 3D perspective projection support