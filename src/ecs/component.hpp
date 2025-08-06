#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <cstring>

// Transform component - consolidates position/rotation for better cache locality
struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    
    // Cached transform matrix - updated when dirty
    mutable glm::mat4 matrix{1.0f};
    mutable bool dirty{true};
    
    const glm::mat4& getMatrix() const {
        if (dirty) {
            matrix = glm::translate(glm::mat4(1.0f), position) *
                    glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0, 0, 1)) *
                    glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0, 1, 0)) *
                    glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1, 0, 0)) *
                    glm::scale(glm::mat4(1.0f), scale);
            dirty = false;
        }
        return matrix;
    }
    
    void setPosition(const glm::vec3& pos) { position = pos; dirty = true; }
    void setRotation(const glm::vec3& rot) { rotation = rot; dirty = true; }
    void setScale(const glm::vec3& scl) { scale = scl; dirty = true; }
};

// Velocity component for physics
struct Velocity {
    glm::vec3 linear{0.0f, 0.0f, 0.0f};
    glm::vec3 angular{0.0f, 0.0f, 0.0f};
};

// Render component - optimized for batch rendering
struct Renderable {
    enum class ShapeType : uint8_t {
        Triangle = 0,
        Square = 1,
        COUNT
    };
    
    ShapeType shape{ShapeType::Triangle};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t layer{0}; // For depth sorting
    bool visible{true};
    
    // Change tracking for optimization
    mutable uint32_t version{0};
    void markDirty() const { ++version; }
};

// Lifetime management component
struct Lifetime {
    float maxAge{-1.0f}; // -1 = infinite
    float currentAge{0.0f};
    bool autoDestroy{false};
};

// Physics bounds for collision detection
struct Bounds {
    glm::vec3 min{-0.5f, -0.5f, -0.5f};
    glm::vec3 max{0.5f, 0.5f, 0.5f};
    bool dynamic{true}; // Updates with transform changes
};

// Fractal movement patterns for beautiful entity motion
enum class MovementType {
    Linear,          // Simple linear movement
    Orbital,         // Circular/elliptical orbits
    Spiral,          // Logarithmic spiral
    Lissajous,       // Complex periodic patterns
    Brownian,        // Random walk
    Fractal,         // Self-similar recursive patterns
    Wave,            // Sine/cosine wave patterns
    Petal,           // Rose curve patterns
    Butterfly        // Butterfly curve
};

struct MovementPattern {
    MovementType type{MovementType::Linear};
    
    // Universal parameters
    float amplitude{1.0f};      // Size/scale of pattern
    float frequency{1.0f};      // Speed/frequency of oscillation
    float phase{0.0f};          // Phase offset for variation
    float timeOffset{0.0f};     // Individual timing offset
    
    // Pattern-specific parameters
    glm::vec3 center{0.0f};     // Center point for orbits/spirals
    glm::vec3 axis{0.0f, 0.0f, 1.0f}; // Rotation axis
    
    // Fractal parameters
    float recursionDepth{3.0f}; // Depth of fractal recursion
    float selfSimilarity{0.618f}; // Golden ratio for aesthetics
    
    // Advanced parameters
    float decay{0.0f};          // Amplitude decay over time
    float phaseShift{0.0f};     // Phase shift rate
    glm::vec2 lissajousRatio{3.0f, 2.0f}; // Frequency ratios for Lissajous
    
    // Runtime state
    mutable float totalTime{0.0f};
    mutable glm::vec3 lastPosition{0.0f};
    mutable bool initialized{false};
};

// Input system components for ECS-based input handling
struct KeyboardInput {
    static constexpr size_t MAX_KEYS = 512;
    
    // Current frame key states
    bool keys[MAX_KEYS] = {false};
    bool keysPressed[MAX_KEYS] = {false};   // Key pressed this frame
    bool keysReleased[MAX_KEYS] = {false};  // Key released this frame
    
    // Modifier states
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    
    // Helper methods
    bool isKeyDown(int scancode) const {
        return scancode >= 0 && static_cast<size_t>(scancode) < MAX_KEYS && keys[scancode];
    }
    
    bool isKeyPressed(int scancode) const {
        return scancode >= 0 && static_cast<size_t>(scancode) < MAX_KEYS && keysPressed[scancode];
    }
    
    bool isKeyReleased(int scancode) const {
        return scancode >= 0 && static_cast<size_t>(scancode) < MAX_KEYS && keysReleased[scancode];
    }
    
    void clearFrameStates() {
        std::memset(keysPressed, false, sizeof(keysPressed));
        std::memset(keysReleased, false, sizeof(keysReleased));
    }
};

struct MouseInput {
    // Mouse position
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 deltaPosition{0.0f, 0.0f};
    glm::vec2 worldPosition{0.0f, 0.0f}; // Position in world coordinates
    
    // Mouse buttons (left, middle, right, x1, x2)
    static constexpr size_t MAX_BUTTONS = 8;
    bool buttons[MAX_BUTTONS] = {false};
    bool buttonsPressed[MAX_BUTTONS] = {false};
    bool buttonsReleased[MAX_BUTTONS] = {false};
    
    // Mouse wheel
    glm::vec2 wheelDelta{0.0f, 0.0f};
    
    // Mouse state
    bool isInWindow = true;
    bool isRelativeMode = false;
    
    // Helper methods
    bool isButtonDown(int button) const {
        return button >= 0 && static_cast<size_t>(button) < MAX_BUTTONS && buttons[button];
    }
    
    bool isButtonPressed(int button) const {
        return button >= 0 && static_cast<size_t>(button) < MAX_BUTTONS && buttonsPressed[button];
    }
    
    bool isButtonReleased(int button) const {
        return button >= 0 && static_cast<size_t>(button) < MAX_BUTTONS && buttonsReleased[button];
    }
    
    void clearFrameStates() {
        std::memset(buttonsPressed, false, sizeof(buttonsPressed));
        std::memset(buttonsReleased, false, sizeof(buttonsReleased));
        wheelDelta = glm::vec2(0.0f, 0.0f);
        deltaPosition = glm::vec2(0.0f, 0.0f);
    }
};

struct InputEvents {
    static constexpr size_t MAX_EVENTS = 64;
    
    struct Event {
        enum Type {
            QUIT,
            KEY_DOWN,
            KEY_UP,
            MOUSE_BUTTON_DOWN,
            MOUSE_BUTTON_UP,
            MOUSE_MOTION,
            MOUSE_WHEEL,
            WINDOW_RESIZE
        };
        
        Type type;
        union {
            struct { int key; bool repeat; } keyEvent;
            struct { int button; glm::vec2 position; } mouseButtonEvent;
            struct { glm::vec2 position; glm::vec2 delta; } mouseMotionEvent;
            struct { glm::vec2 delta; } mouseWheelEvent;
            struct { int width; int height; } windowResizeEvent;
        };
    };
    
    Event events[MAX_EVENTS];
    size_t eventCount = 0;
    
    void addEvent(const Event& event) {
        if (eventCount < MAX_EVENTS) {
            events[eventCount++] = event;
        }
    }
    
    void clear() {
        eventCount = 0;
    }
};

// Singleton component for global input state
struct InputState {
    bool quit = false;
    float deltaTime = 0.0f;
    uint32_t frameNumber = 0;
    
    // Input processing flags
    bool processKeyboard = true;
    bool processMouse = true;
    bool consumeEvents = true; // Whether to consume SDL events
};

// Camera component for 2D view control
struct Camera {
    glm::vec3 position{0.0f, 0.0f, 0.0f};  // Camera position in world space
    float zoom{1.0f};                       // Zoom level (1.0 = normal, <1.0 = zoomed out, >1.0 = zoomed in)
    float rotation{0.0f};                   // Camera rotation in radians around Z-axis
    
    // View bounds (calculated from position/zoom)
    glm::vec2 viewSize{8.0f, 6.0f};        // Base view size (width, height)
    float aspectRatio{4.0f / 3.0f};        // Aspect ratio to maintain
    
    // Movement controls
    float moveSpeed{5.0f};                  // Units per second
    float zoomSpeed{2.0f};                  // Zoom factor per second
    float rotationSpeed{1.0f};              // Radians per second
    
    // Zoom limits
    float minZoom{0.1f};
    float maxZoom{10.0f};
    
    // Cached matrices (updated when dirty)
    mutable glm::mat4 viewMatrix{1.0f};
    mutable glm::mat4 projectionMatrix{1.0f};
    mutable bool viewDirty{true};
    mutable bool projectionDirty{true};
    
    // Get view matrix (lazy computation)
    const glm::mat4& getViewMatrix() const {
        if (viewDirty) {
            // Create view matrix: translate to camera position, then rotate
            viewMatrix = glm::rotate(glm::mat4(1.0f), -rotation, glm::vec3(0, 0, 1));
            viewMatrix = glm::translate(viewMatrix, -position);
            viewDirty = false;
        }
        return viewMatrix;
    }
    
    // Get projection matrix (lazy computation)
    const glm::mat4& getProjectionMatrix() const {
        if (projectionDirty) {
            // Calculate actual view size based on zoom
            float actualWidth = viewSize.x / zoom;
            float actualHeight = viewSize.y / zoom;
            
            // Create orthographic projection centered on camera
            float left = -actualWidth * 0.5f;
            float right = actualWidth * 0.5f;
            float bottom = -actualHeight * 0.5f;
            float top = actualHeight * 0.5f;
            
            projectionMatrix = glm::ortho(left, right, bottom, top, -5.0f, 5.0f);
            projectionMatrix[1][1] *= -1; // Flip Y for Vulkan
            projectionDirty = false;
        }
        return projectionMatrix;
    }
    
    // Movement functions
    void setPosition(const glm::vec3& pos) {
        position = pos;
        viewDirty = true;
    }
    
    void move(const glm::vec3& delta) {
        position += delta;
        viewDirty = true;
    }
    
    void setZoom(float newZoom) {
        zoom = glm::clamp(newZoom, minZoom, maxZoom);
        projectionDirty = true;
    }
    
    void adjustZoom(float zoomDelta) {
        setZoom(zoom * zoomDelta);
    }
    
    void setRotation(float newRotation) {
        rotation = newRotation;
        viewDirty = true;
    }
    
    void rotate(float rotationDelta) {
        rotation += rotationDelta;
        viewDirty = true;
    }
    
    // Set aspect ratio and update projection
    void setAspectRatio(float ratio) {
        aspectRatio = ratio;
        viewSize.y = viewSize.x / ratio;
        projectionDirty = true;
    }
    
    // Convert screen coordinates to world coordinates
    glm::vec2 screenToWorld(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
        // Normalize screen coordinates to [-1, 1]
        glm::vec2 normalized;
        normalized.x = (screenPos.x / screenSize.x) * 2.0f - 1.0f;
        normalized.y = 1.0f - (screenPos.y / screenSize.y) * 2.0f; // Flip Y
        
        // Apply inverse projection and view transformations
        glm::vec4 worldPos = glm::inverse(getProjectionMatrix() * getViewMatrix()) * glm::vec4(normalized, 0.0f, 1.0f);
        return glm::vec2(worldPos.x, worldPos.y);
    }
    
    // Check if a world position is visible
    bool isVisible(const glm::vec3& worldPos) const {
        float actualWidth = viewSize.x / zoom;
        float actualHeight = viewSize.y / zoom;
        
        return (worldPos.x >= position.x - actualWidth * 0.5f && 
                worldPos.x <= position.x + actualWidth * 0.5f &&
                worldPos.y >= position.y - actualHeight * 0.5f && 
                worldPos.y <= position.y + actualHeight * 0.5f);
    }
};

// Tag components for input-responsive entities
struct KeyboardControlled {};   // Entity responds to keyboard input
struct MouseControlled {};      // Entity responds to mouse input
struct InputResponsive {};      // Entity responds to any input

// Tag components for efficient filtering
struct Static {}; // Non-moving entities
struct Dynamic {}; // Moving entities  
struct Pooled {}; // Can be recycled

// Backward compatibility aliases
using Position = Transform;
using Color = Renderable;
using Shape = Renderable;