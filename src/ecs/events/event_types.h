#pragma once

#include "../component.h"
#include <glm/glm.hpp>
#include <flecs.h>
#include <string>
#include <vector>
#include <cstdint>
#include <SDL3/SDL.h>

namespace Events {

// Forward declarations
class EventBus;

// =============================================================================
// INPUT EVENTS
// =============================================================================

struct KeyboardEvent {
    int scancode = 0;           // SDL scancode
    int keycode = 0;            // SDL keycode (virtual key)
    uint16_t modifiers = 0;     // Key modifier state (KMOD_*)
    bool pressed = false;       // true for press, false for release
    bool repeat = false;        // true if this is a key repeat
    std::string keyName;        // Human-readable key name
    
    // Common key helper methods
    bool isModifierPressed(uint16_t modifier) const { return (modifiers & modifier) != 0; }
    bool isShiftPressed() const { return isModifierPressed(SDL_KMOD_SHIFT); }
    bool isCtrlPressed() const { return isModifierPressed(SDL_KMOD_CTRL); }
    bool isAltPressed() const { return isModifierPressed(SDL_KMOD_ALT); }
    bool isGuiPressed() const { return isModifierPressed(SDL_KMOD_GUI); }
};

struct MouseButtonEvent {
    int button = 0;             // Mouse button (SDL_BUTTON_*)
    glm::vec2 position{0.0f};   // Mouse position in screen coordinates
    glm::vec2 worldPosition{0.0f}; // Mouse position in world coordinates
    bool pressed = false;       // true for press, false for release
    int clicks = 1;             // Number of clicks (1 = single, 2 = double, etc.)
    
    // Common button helper methods
    bool isLeftButton() const { return button == SDL_BUTTON_LEFT; }
    bool isRightButton() const { return button == SDL_BUTTON_RIGHT; }
    bool isMiddleButton() const { return button == SDL_BUTTON_MIDDLE; }
};

struct MouseMotionEvent {
    glm::vec2 position{0.0f};       // Current mouse position in screen coordinates
    glm::vec2 worldPosition{0.0f};  // Current mouse position in world coordinates
    glm::vec2 delta{0.0f};          // Movement delta since last event
    glm::vec2 worldDelta{0.0f};     // Movement delta in world coordinates
    bool dragInProgress = false;    // true if any mouse button is held down
    uint32_t buttonsMask = 0;       // Bitmask of currently pressed buttons
};

struct MouseWheelEvent {
    glm::vec2 scroll{0.0f};         // Scroll amount (x = horizontal, y = vertical)
    glm::vec2 position{0.0f};       // Mouse position when scrolling
    glm::vec2 worldPosition{0.0f};  // Mouse position in world coordinates
    bool flipped = false;           // true if scroll values should be flipped
    bool preciseScrolling = false;  // true for high-precision scrolling
};

struct TextInputEvent {
    std::string text;               // UTF-8 encoded text input
    int32_t cursor = 0;             // Cursor position within the text
    int32_t selectionLength = 0;    // Length of selected text
};

// Composite input state event for convenience
struct InputStateEvent {
    const KeyboardInput* keyboard = nullptr;   // Current keyboard state
    const MouseInput* mouse = nullptr;         // Current mouse state
    float deltaTime = 0.0f;                   // Time since last frame
    uint32_t frameNumber = 0;                 // Current frame number
};

// =============================================================================
// ENTITY LIFECYCLE EVENTS
// =============================================================================

struct EntityCreatedEvent {
    flecs::entity entity;          // The created entity
    std::string entityName;        // Optional entity name/tag
    std::vector<std::string> components; // List of component type names
    glm::vec3 initialPosition{0.0f}; // Initial position if applicable
    void* userData = nullptr;       // Optional user data
};

struct EntityDestroyedEvent {
    flecs::entity entity;          // The entity being destroyed (may be invalid by event time)
    uint64_t entityId = 0;         // Entity ID for reference after destruction
    std::string entityName;        // Entity name/tag if it had one
    std::vector<std::string> components; // Components that were attached
    float lifetime = 0.0f;         // How long the entity existed
};

struct ComponentAddedEvent {
    flecs::entity entity;          // The entity that received the component
    std::string componentType;     // Type name of the added component
    const void* componentData = nullptr; // Pointer to component data (use with caution)
    size_t componentSize = 0;      // Size of the component data
};

struct ComponentRemovedEvent {
    flecs::entity entity;          // The entity that lost the component
    std::string componentType;     // Type name of the removed component
    // Note: component data is no longer available at this point
};

struct ComponentChangedEvent {
    flecs::entity entity;          // The entity with the changed component
    std::string componentType;     // Type name of the changed component
    const void* oldData = nullptr; // Previous component data (may be null)
    const void* newData = nullptr; // New component data
    size_t componentSize = 0;      // Size of the component data
    uint32_t changeFlags = 0;      // Bitmask indicating which fields changed
};

// =============================================================================
// PHYSICS EVENTS
// =============================================================================

struct CollisionEvent {
    flecs::entity entityA;         // First entity in collision
    flecs::entity entityB;         // Second entity in collision
    glm::vec3 contactPoint{0.0f};   // Point of collision in world space
    glm::vec3 normal{0.0f};         // Collision normal (from A to B)
    float penetrationDepth = 0.0f;  // How deep the collision is
    float relativeVelocity = 0.0f;  // Relative velocity at collision point
    bool isTrigger = false;         // true if this is a trigger collision
    float impulse = 0.0f;           // Collision impulse magnitude
};

struct TriggerEvent {
    flecs::entity triggerEntity;    // The trigger entity
    flecs::entity otherEntity;      // The entity that entered/exited the trigger
    bool entered = true;            // true for enter, false for exit
    glm::vec3 contactPoint{0.0f};   // Point of contact
    std::string triggerName;        // Optional trigger identifier
};

struct PhysicsStepEvent {
    float deltaTime = 0.0f;         // Physics step time
    float timeAccumulator = 0.0f;   // Accumulated simulation time
    uint32_t stepCount = 0;         // Number of physics steps this frame
    size_t activeEntities = 0;      // Number of entities with physics
};

// =============================================================================
// CAMERA EVENTS
// =============================================================================

struct CameraPositionChangedEvent {
    flecs::entity cameraEntity;    // The camera entity
    glm::vec3 oldPosition{0.0f};    // Previous camera position
    glm::vec3 newPosition{0.0f};    // New camera position
    glm::vec3 velocity{0.0f};       // Camera movement velocity
};

struct CameraRotationChangedEvent {
    flecs::entity cameraEntity;    // The camera entity
    glm::vec3 oldRotation{0.0f};    // Previous camera rotation
    glm::vec3 newRotation{0.0f};    // New camera rotation
    glm::vec3 angularVelocity{0.0f}; // Camera rotation velocity
};

struct CameraZoomChangedEvent {
    flecs::entity cameraEntity;    // The camera entity
    float oldZoom = 1.0f;           // Previous zoom level
    float newZoom = 1.0f;           // New zoom level
    float zoomSpeed = 0.0f;         // Rate of zoom change
    glm::vec2 zoomCenter{0.0f};     // Point being zoomed towards (screen coords)
};

struct CameraViewChangedEvent {
    flecs::entity cameraEntity;    // The camera entity
    glm::mat4 viewMatrix{1.0f};     // New view matrix
    glm::mat4 projectionMatrix{1.0f}; // Current projection matrix
    glm::mat4 viewProjectionMatrix{1.0f}; // Combined view-projection matrix
    glm::vec4 viewport{0.0f};       // Viewport (x, y, width, height)
};

struct CameraBoundsChangedEvent {
    flecs::entity cameraEntity;    // The camera entity
    glm::vec3 minBounds{0.0f};      // Minimum world bounds visible
    glm::vec3 maxBounds{0.0f};      // Maximum world bounds visible
    float nearPlane = 0.1f;         // Near clipping plane
    float farPlane = 1000.0f;       // Far clipping plane
};

// =============================================================================
// SYSTEM EVENTS
// =============================================================================

struct SystemInitializedEvent {
    std::string systemName;         // Name of the initialized system
    std::string systemType;         // Type/category of system
    float initializationTime = 0.0f; // Time taken to initialize (seconds)
    bool success = true;            // Whether initialization succeeded
    std::string errorMessage;       // Error message if initialization failed
};

struct SystemShutdownEvent {
    std::string systemName;         // Name of the shutdown system
    std::string systemType;         // Type/category of system
    float shutdownTime = 0.0f;      // Time taken to shutdown (seconds)
    float uptime = 0.0f;           // How long the system was active
    bool graceful = true;           // Whether shutdown was graceful
};

struct ModuleLoadedEvent {
    std::string moduleName;         // Name of the loaded module
    std::string moduleVersion;      // Module version string
    float loadTime = 0.0f;          // Time taken to load (seconds)
    std::vector<std::string> dependencies; // Module dependencies
    std::vector<std::string> providedSystems; // Systems provided by module
};

struct ModuleUnloadedEvent {
    std::string moduleName;         // Name of the unloaded module
    std::string moduleVersion;      // Module version string
    float uptime = 0.0f;           // How long module was active
    bool graceful = true;           // Whether unload was graceful
    std::string reason;             // Reason for unloading
};

// =============================================================================
// RENDERING EVENTS
// =============================================================================

struct FrameStartEvent {
    uint32_t frameNumber = 0;       // Current frame number
    float deltaTime = 0.0f;         // Time since last frame
    float totalTime = 0.0f;         // Total elapsed time
    size_t visibleEntities = 0;     // Number of entities to render
};

struct FrameEndEvent {
    uint32_t frameNumber = 0;       // Completed frame number
    float frameTime = 0.0f;         // Time to render this frame
    float fps = 0.0f;              // Current FPS
    size_t entitiesRendered = 0;    // Number of entities actually rendered
    size_t drawCalls = 0;           // Number of draw calls issued
};

struct RenderPassEvent {
    std::string passName;           // Name of the render pass
    uint32_t passIndex = 0;         // Index of this pass in the frame
    float passTime = 0.0f;          // Time taken for this pass
    size_t entitiesProcessed = 0;   // Entities processed in this pass
};

struct SwapchainRecreatedEvent {
    glm::uvec2 oldSize{0};          // Previous swapchain size
    glm::uvec2 newSize{0};          // New swapchain size
    bool sizeChanged = true;        // Whether size actually changed
    std::string reason;             // Reason for recreation (resize, format change, etc.)
};

// =============================================================================
// PERFORMANCE EVENTS
// =============================================================================

struct PerformanceWarningEvent {
    std::string subsystem;          // System that generated the warning
    std::string warningType;        // Type of performance issue
    std::string message;            // Detailed warning message
    float threshold = 0.0f;         // Performance threshold that was exceeded
    float actualValue = 0.0f;       // Actual measured value
    std::string suggestedAction;    // Suggested action to resolve issue
};

struct MemoryPressureEvent {
    size_t totalMemory = 0;         // Total system memory
    size_t availableMemory = 0;     // Available system memory
    size_t usedMemory = 0;          // Memory used by application
    size_t gpuMemoryUsed = 0;       // GPU memory usage
    size_t gpuMemoryAvailable = 0;  // Available GPU memory
    float pressureLevel = 0.0f;     // Pressure level (0.0 = no pressure, 1.0 = critical)
};

// =============================================================================
// APPLICATION EVENTS
// =============================================================================

struct ApplicationStartedEvent {
    std::string applicationName;    // Application name
    std::string version;            // Application version
    float startupTime = 0.0f;       // Time taken to start up
    std::vector<std::string> commandLineArgs; // Command line arguments
};

struct ApplicationQuitEvent {
    std::string reason;             // Reason for quitting
    bool userInitiated = true;      // Whether user initiated quit
    bool graceful = true;           // Whether shutdown is graceful
    float uptime = 0.0f;           // How long application ran
};

struct WindowResizeEvent {
    glm::uvec2 oldSize{0};          // Previous window size
    glm::uvec2 newSize{0};          // New window size
    bool minimized = false;         // Whether window is minimized
    bool maximized = false;         // Whether window is maximized
    bool fullscreen = false;        // Whether window is fullscreen
};

struct WindowFocusEvent {
    bool gained = true;             // true for focus gained, false for lost
    float timeSinceLastFocus = 0.0f; // Time since focus was last changed
};

// =============================================================================
// AUDIO EVENTS (for future extension)
// =============================================================================

struct AudioEvent {
    std::string soundName;          // Name/ID of the sound
    glm::vec3 position{0.0f};       // 3D position of sound source
    float volume = 1.0f;            // Volume level (0.0 to 1.0)
    float pitch = 1.0f;             // Pitch multiplier
    bool looping = false;           // Whether sound should loop
    uint32_t sourceId = 0;          // Audio source identifier
};

// =============================================================================
// NETWORKING EVENTS (for future extension)
// =============================================================================

struct NetworkEvent {
    std::string eventType;          // Type of network event
    std::string endpoint;           // Network endpoint (IP:port, etc.)
    std::vector<uint8_t> data;      // Raw network data
    size_t dataSize = 0;            // Size of data
    float latency = 0.0f;           // Network latency if applicable
    bool isIncoming = true;         // true for incoming, false for outgoing
};

// =============================================================================
// DEBUG AND PROFILING EVENTS
// =============================================================================

struct DebugMessageEvent {
    enum class Severity {
        Info,
        Warning,
        Error,
        Critical
    };
    
    Severity severity = Severity::Info; // Message severity
    std::string category;           // Debug category (rendering, physics, etc.)
    std::string message;            // Debug message
    std::string file;               // Source file (if available)
    int line = 0;                  // Source line (if available)
    std::string function;           // Function name (if available)
};

struct ProfilerEvent {
    std::string profileName;        // Name of profiled section
    float duration = 0.0f;          // Duration in seconds
    float percentage = 0.0f;        // Percentage of frame time
    uint32_t callCount = 1;         // Number of calls this frame
    std::string category;           // Profiler category
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

namespace Utility {
    // Helper function to convert SDL events to our event types
    std::vector<std::unique_ptr<Events::BaseEvent>> convertSDLEvent(const SDL_Event& sdlEvent);
    
    // Helper to publish common events easily
    template<typename EventType>
    void publishEvent(EventBus& eventBus, const EventType& eventData) {
        eventBus.publish<EventType>(eventData);
    }
    
    // Batch event publishing for performance
    template<typename EventType>
    void publishEvents(EventBus& eventBus, const std::vector<EventType>& events) {
        for (const auto& event : events) {
            eventBus.publish<EventType>(event);
        }
    }
}

} // namespace Events