#pragma once

#include "../core/service_locator.h"
#include "camera/camera_manager.h"
#include "camera/camera_transition_system.h"
#include "camera/viewport_manager.h" 
#include "camera/camera_culling.h"
#include "camera/camera_transforms.h"
#include "../components/component.h"
#include <flecs.h>
#include <memory>

class CameraService {
public:
    DECLARE_SERVICE(CameraService);
    
    CameraService();
    ~CameraService();
    
    // Non-copyable due to unique_ptr members
    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;
    CameraService(CameraService&&) = default;
    CameraService& operator=(CameraService&&) = default;
    
    bool initialize(flecs::world& world);
    void cleanup();
    
    void update(float deltaTime);
    void handleWindowResize(int width, int height);
    
    CameraID createCamera(const std::string& name = "");
    CameraID createCamera(const Camera& cameraData, const std::string& name = "");
    bool removeCamera(CameraID cameraID);
    bool hasCamera(CameraID cameraID) const;
    
    Camera* getCamera(CameraID cameraID);
    const Camera* getCamera(CameraID cameraID) const;
    Camera* getCameraByName(const std::string& name);
    const Camera* getCameraByName(const std::string& name) const;
    flecs::entity getCameraEntity(CameraID cameraID);
    
    void setActiveCamera(CameraID cameraID);
    CameraID getActiveCameraID() const;
    Camera* getActiveCameraData();
    const Camera* getActiveCameraData() const;
    
    void transitionToCamera(CameraID targetCameraID, const CameraTransition& transition);
    void transitionToCamera(CameraID targetCameraID, CameraTransitionType type, float duration);
    void transitionActiveCameraTo(const Camera& targetState, const CameraTransition& transition);
    void transitionActiveCameraTo(const Camera& targetState, CameraTransitionType type, float duration);
    bool isTransitionActive() const;
    void cancelTransition();
    
    void createViewport(const std::string& name, CameraID cameraID, const glm::vec2& offset, const glm::vec2& size);
    void createViewport(const Viewport& viewport);
    void removeViewport(const std::string& name);
    void setViewportActive(const std::string& name, bool active);
    bool hasViewport(const std::string& name) const;
    Viewport* getViewport(const std::string& name);
    const Viewport* getViewport(const std::string& name) const;
    std::vector<Viewport*> getActiveViewports();
    std::vector<const Viewport*> getActiveViewports() const;
    
    std::vector<CullingInfo> performFrustumCulling(const std::vector<Transform>& transforms, 
                                                   const std::vector<Bounds>& bounds,
                                                   CameraID cameraID = 0) const;
    bool isEntityVisible(const Transform& transform, const Bounds& bounds, CameraID cameraID = 0) const;
    
    int calculateLODLevel(const glm::vec3& entityPosition, CameraID cameraID = 0) const;
    void setLODDistances(const std::vector<float>& distances);
    const std::vector<float>& getLODDistances() const;
    
    glm::vec2 worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize, CameraID cameraID = 0) const;
    glm::vec2 screenToWorld(const glm::vec2& screenPos, const glm::vec2& screenSize, CameraID cameraID = 0) const;
    glm::vec2 viewportToWorld(const glm::vec2& viewportPos, const std::string& viewportName) const;
    
    glm::vec3 getCameraPosition(CameraID cameraID = 0) const;
    float getCameraZoom(CameraID cameraID = 0) const;
    float getCameraRotation(CameraID cameraID = 0) const;
    glm::mat4 getViewMatrix(CameraID cameraID = 0) const;
    glm::mat4 getProjectionMatrix(CameraID cameraID = 0) const;
    glm::mat4 getViewProjectionMatrix(CameraID cameraID = 0) const;
    
    using CameraBounds = CameraCulling::CameraBounds;
    CameraBounds getCameraBounds(CameraID cameraID = 0) const;
    bool isPositionVisible(const glm::vec3& position, CameraID cameraID = 0) const;
    
    std::vector<CameraID> getAllCameraIDs() const;
    std::vector<std::string> getCameraNames() const;
    CameraID findNearestCamera(const glm::vec3& position) const;
    
    void moveCamera(CameraID cameraID, const glm::vec3& delta);
    void setCameraPosition(CameraID cameraID, const glm::vec3& position);
    void setCameraZoom(CameraID cameraID, float zoom);
    void setCameraRotation(CameraID cameraID, float rotation);
    void focusCameraOn(CameraID cameraID, const glm::vec3& target, float zoom = 0.0f);
    
    void createOrthographicCamera(const std::string& name, const glm::vec3& position, float zoom, const glm::vec2& viewSize);
    void createFollowCamera(const std::string& name, const glm::vec3& target, float distance, float zoom);
    
    void printCameraInfo(CameraID cameraID = 0) const;
    void printAllCameras() const;
    void printViewportInfo() const;
    
    void setDefaultTransitionType(CameraTransitionType type);
    void setDefaultTransitionDuration(float duration);
    CameraTransitionType getDefaultTransitionType() const;
    float getDefaultTransitionDuration() const;

private:
    bool initialized = false;
    
    std::unique_ptr<CameraManager> cameraManager;
    std::unique_ptr<CameraTransitionSystem> transitionSystem;
    std::unique_ptr<ViewportManager> viewportManager;
    std::unique_ptr<CameraCulling> culling;
    std::unique_ptr<CameraTransforms> transforms;
    
    void updateActiveCamera();
    const Camera* getCameraForOperations(CameraID cameraID) const;
    Camera* getCameraForOperations(CameraID cameraID);
};

