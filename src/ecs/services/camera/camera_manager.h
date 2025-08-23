#pragma once

#include "../../components/camera_component.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using CameraID = uint32_t;

class CameraManager {
public:
    CameraManager() = default;
    ~CameraManager() = default;

    bool initialize(flecs::world& world);
    void cleanup();

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
    CameraID getActiveCameraID() const { return activeCameraID; }
    Camera* getActiveCameraData();
    const Camera* getActiveCameraData() const;

    void handleWindowResize(int width, int height);

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

private:
    flecs::world* world = nullptr;
    bool initialized = false;
    
    std::unordered_map<CameraID, flecs::entity> cameras;
    std::unordered_map<std::string, CameraID> cameraNames;
    CameraID nextCameraID = 1;
    CameraID activeCameraID = 0;
    
    glm::vec2 windowSize{800.0f, 600.0f};
    
    void ensureMainCamera();
    flecs::entity createCameraEntity(const Camera& cameraData, const std::string& name);
    flecs::entity createMainCameraEntity();
};