#pragma once

#include "../core/service_locator.h"
#include "../components/component.h"
#include "../components/camera_component.h"
#include "../systems/camera_system.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

// Camera identifier type
using CameraID = uint32_t;

// Camera transition types
enum class CameraTransitionType {
    INSTANT,        // No transition
    LINEAR,         // Linear interpolation
    SMOOTH_STEP,    // Smooth step interpolation
    EASE_IN,        // Ease in
    EASE_OUT,       // Ease out
    EASE_IN_OUT,    // Ease in-out
    SPRING          // Spring-based smooth transition
};

// Camera transition data
struct CameraTransition {
    CameraTransitionType type = CameraTransitionType::INSTANT;
    float duration = 1.0f;          // Duration in seconds
    float currentTime = 0.0f;       // Current transition time
    bool active = false;            // Is transition active
    
    // Start and end states for interpolation
    Camera startState;
    Camera endState;
    
    // Custom easing function (optional)
    std::function<float(float)> customEasing;
    
    // Callback when transition completes
    std::function<void()> onComplete;
};

// Viewport definition for split-screen support
struct Viewport {
    std::string name;
    CameraID cameraID = 0;
    
    // Normalized viewport coordinates [0, 1]
    glm::vec2 offset{0.0f, 0.0f};     // Top-left corner
    glm::vec2 size{1.0f, 1.0f};       // Width and height
    
    // Viewport settings
    bool active = true;
    int renderOrder = 0;              // Lower values render first
    glm::vec4 clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    bool clearColorBuffer = true;
    bool clearDepthBuffer = true;
    
    // Transform viewport coordinates to screen coordinates
    glm::vec4 getScreenRect(const glm::vec2& screenSize) const {
        return glm::vec4(
            offset.x * screenSize.x,
            offset.y * screenSize.y,
            size.x * screenSize.x,
            size.y * screenSize.y
        );
    }
    
    // Check if screen point is within this viewport
    bool containsPoint(const glm::vec2& screenPoint, const glm::vec2& screenSize) const {
        glm::vec4 rect = getScreenRect(screenSize);
        return screenPoint.x >= rect.x && screenPoint.x <= rect.x + rect.z &&
               screenPoint.y >= rect.y && screenPoint.y <= rect.y + rect.w;
    }
};

// Culling information
struct CullingInfo {
    glm::vec3 position;
    glm::vec3 bounds;    // Half extents
    bool visible = true;
    float distanceToCamera = 0.0f;
    int lodLevel = 0;    // Level of Detail level
};

// Camera service for advanced camera management
class CameraService {
public:
    DECLARE_SERVICE(CameraService);
    
    CameraService();
    ~CameraService();
    
    // Initialization and cleanup
    bool initialize(flecs::world& world);
    void cleanup();
    
    // Frame processing
    void update(float deltaTime);
    void handleWindowResize(int width, int height);
    
    // Camera management
    CameraID createCamera(const std::string& name = "");
    CameraID createCamera(const Camera& cameraData, const std::string& name = "");
    bool removeCamera(CameraID cameraID);
    bool hasCamera(CameraID cameraID) const;
    
    // Camera access
    Camera* getCamera(CameraID cameraID);
    const Camera* getCamera(CameraID cameraID) const;
    Camera* getCameraByName(const std::string& name);
    const Camera* getCameraByName(const std::string& name) const;
    flecs::entity getCameraEntity(CameraID cameraID);
    
    // Active camera management
    void setActiveCamera(CameraID cameraID);
    CameraID getActiveCameraID() const { return activeCameraID; }
    Camera* getActiveCameraData();
    const Camera* getActiveCameraData() const;
    
    // Camera transitions
    void transitionToCamera(CameraID targetCameraID, const CameraTransition& transition);
    void transitionToCamera(CameraID targetCameraID, CameraTransitionType type, float duration);
    void transitionActiveCameraTo(const Camera& targetState, const CameraTransition& transition);
    void transitionActiveCameraTo(const Camera& targetState, CameraTransitionType type, float duration);
    bool isTransitionActive() const { return currentTransition.active; }
    void cancelTransition();
    
    // Viewport management (split-screen support)
    void createViewport(const std::string& name, CameraID cameraID, const glm::vec2& offset, const glm::vec2& size);
    void createViewport(const Viewport& viewport);
    void removeViewport(const std::string& name);
    void setViewportActive(const std::string& name, bool active);
    bool hasViewport(const std::string& name) const;
    Viewport* getViewport(const std::string& name);
    const Viewport* getViewport(const std::string& name) const;
    std::vector<Viewport*> getActiveViewports();
    std::vector<const Viewport*> getActiveViewports() const;
    
    // Camera culling integration
    std::vector<CullingInfo> performFrustumCulling(const std::vector<Transform>& transforms, 
                                                   const std::vector<Bounds>& bounds,
                                                   CameraID cameraID = 0) const; // 0 = active camera
    bool isEntityVisible(const Transform& transform, const Bounds& bounds, CameraID cameraID = 0) const;
    
    // LOD (Level of Detail) management
    int calculateLODLevel(const glm::vec3& entityPosition, CameraID cameraID = 0) const;
    void setLODDistances(const std::vector<float>& distances) { lodDistances = distances; }
    const std::vector<float>& getLODDistances() const { return lodDistances; }
    
    // Coordinate transformations
    glm::vec2 worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize, CameraID cameraID = 0) const;
    glm::vec2 screenToWorld(const glm::vec2& screenPos, const glm::vec2& screenSize, CameraID cameraID = 0) const;
    glm::vec2 viewportToWorld(const glm::vec2& viewportPos, const std::string& viewportName) const;
    
    // Camera queries and utilities
    glm::vec3 getCameraPosition(CameraID cameraID = 0) const;
    float getCameraZoom(CameraID cameraID = 0) const;
    float getCameraRotation(CameraID cameraID = 0) const;
    glm::mat4 getViewMatrix(CameraID cameraID = 0) const;
    glm::mat4 getProjectionMatrix(CameraID cameraID = 0) const;
    glm::mat4 getViewProjectionMatrix(CameraID cameraID = 0) const;
    
    // Camera bounds and visibility
    struct CameraBounds {
        glm::vec2 min;
        glm::vec2 max;
        bool valid = false;
    };
    CameraBounds getCameraBounds(CameraID cameraID = 0) const;
    bool isPositionVisible(const glm::vec3& position, CameraID cameraID = 0) const;
    
    // Multi-camera utilities
    std::vector<CameraID> getAllCameraIDs() const;
    std::vector<std::string> getCameraNames() const;
    CameraID findNearestCamera(const glm::vec3& position) const;
    
    // Camera movement and control helpers
    void moveCamera(CameraID cameraID, const glm::vec3& delta);
    void setCameraPosition(CameraID cameraID, const glm::vec3& position);
    void setCameraZoom(CameraID cameraID, float zoom);
    void setCameraRotation(CameraID cameraID, float rotation);
    void focusCameraOn(CameraID cameraID, const glm::vec3& target, float zoom = 0.0f); // 0 = keep current
    
    // Predefined camera presets
    void createOrthographicCamera(const std::string& name, const glm::vec3& position, float zoom, const glm::vec2& viewSize);
    void createFollowCamera(const std::string& name, const glm::vec3& target, float distance, float zoom);
    
    // Debug and introspection
    void printCameraInfo(CameraID cameraID = 0) const;
    void printAllCameras() const;
    void printViewportInfo() const;
    
    // Configuration
    void setDefaultTransitionType(CameraTransitionType type) { defaultTransitionType = type; }
    void setDefaultTransitionDuration(float duration) { defaultTransitionDuration = duration; }
    CameraTransitionType getDefaultTransitionType() const { return defaultTransitionType; }
    float getDefaultTransitionDuration() const { return defaultTransitionDuration; }

private:
    // Core data
    flecs::world* world = nullptr;
    bool initialized = false;
    
    // Camera management
    std::unordered_map<CameraID, flecs::entity> cameras;
    std::unordered_map<std::string, CameraID> cameraNames;
    CameraID nextCameraID = 1;
    CameraID activeCameraID = 0;
    
    // Viewport management
    std::unordered_map<std::string, Viewport> viewports;
    
    // Transition system
    CameraTransition currentTransition;
    CameraTransitionType defaultTransitionType = CameraTransitionType::LINEAR;
    float defaultTransitionDuration = 1.0f;
    
    // LOD system
    std::vector<float> lodDistances = {10.0f, 50.0f, 100.0f, 500.0f}; // Distance thresholds for LOD levels
    
    // Window properties
    glm::vec2 windowSize{800.0f, 600.0f};
    
    // Internal helpers
    void updateTransition(float deltaTime);
    float evaluateEasing(float t, CameraTransitionType type) const;
    Camera interpolateCameras(const Camera& start, const Camera& end, float t) const;
    void ensureMainCamera();
    flecs::entity createCameraEntity(const Camera& cameraData, const std::string& name);
    
    // Culling helpers
    bool isInFrustum(const glm::vec3& position, const glm::vec3& bounds, const Camera& camera) const;
    float calculateDistanceToCamera(const glm::vec3& position, const Camera& camera) const;
};

// Convenience functions for global access
namespace CameraSystem {
    CameraService& getService();
    
    // Quick camera access
    Camera* getActiveCamera();
    const Camera* getActiveCameraConst();
    CameraID getActiveCameraID();
    void setActiveCamera(CameraID cameraID);
    
    // Quick transformations
    glm::vec2 worldToScreen(const glm::vec3& worldPos);
    glm::vec2 screenToWorld(const glm::vec2& screenPos);
    
    // Quick queries
    bool isVisible(const glm::vec3& position);
    glm::vec3 getCameraPosition();
    float getCameraZoom();
    
    // Quick camera control
    void moveCamera(const glm::vec3& delta);
    void setCameraPosition(const glm::vec3& position);
    void setCameraZoom(float zoom);
    void focusOn(const glm::vec3& target, float zoom = 0.0f);
    
    // Transition shortcuts
    void transitionTo(const glm::vec3& position, float zoom, float duration = 1.0f);
    void transitionTo(CameraID cameraID, float duration = 1.0f);
}