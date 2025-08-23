#include "camera_manager.h"
#include <iostream>
#include <algorithm>
#include <cmath>

bool CameraManager::initialize(flecs::world& world) {
    if (initialized) {
        return true;
    }
    
    this->world = &world;
    initialized = true;  // Set initialized BEFORE calling ensureMainCamera
    
    ensureMainCamera();
    
    return true;
}

void CameraManager::cleanup() {
    if (!initialized) {
        return;
    }
    
    cameras.clear();
    cameraNames.clear();
    
    activeCameraID = 0;
    nextCameraID = 1;
    
    world = nullptr;
    initialized = false;
}

CameraID CameraManager::createCamera(const std::string& name) {
    Camera defaultCamera;
    return createCamera(defaultCamera, name);
}

CameraID CameraManager::createCamera(const Camera& cameraData, const std::string& name) {
    if (!initialized || !world) {
        return 0;
    }
    
    CameraID newID = nextCameraID++;
    
    flecs::entity cameraEntity = createCameraEntity(cameraData, name);
    if (!cameraEntity.is_valid()) {
        return 0;
    }
    
    cameras[newID] = cameraEntity;
    
    if (!name.empty()) {
        cameraNames[name] = newID;
    }
    
    if (activeCameraID == 0) {
        activeCameraID = newID;
    }
    
    return newID;
}

bool CameraManager::removeCamera(CameraID cameraID) {
    if (!initialized || cameraID == 0) {
        return false;
    }
    
    auto it = cameras.find(cameraID);
    if (it == cameras.end()) {
        return false;
    }
    
    flecs::entity entity = it->second;
    cameras.erase(it);
    
    for (auto nameIt = cameraNames.begin(); nameIt != cameraNames.end();) {
        if (nameIt->second == cameraID) {
            nameIt = cameraNames.erase(nameIt);
        } else {
            ++nameIt;
        }
    }
    
    if (entity.is_valid()) {
        entity.destruct();
    }
    
    if (activeCameraID == cameraID) {
        activeCameraID = cameras.empty() ? 0 : cameras.begin()->first;
    }
    
    return true;
}

bool CameraManager::hasCamera(CameraID cameraID) const {
    return cameras.find(cameraID) != cameras.end();
}

Camera* CameraManager::getCamera(CameraID cameraID) {
    if (cameraID == 0) {
        cameraID = activeCameraID;
    }
    
    auto it = cameras.find(cameraID);
    if (it == cameras.end() || !it->second.is_valid()) {
        return nullptr;
    }
    
    return it->second.get_mut<Camera>();
}

const Camera* CameraManager::getCamera(CameraID cameraID) const {
    if (cameraID == 0) {
        cameraID = activeCameraID;
    }
    
    auto it = cameras.find(cameraID);
    if (it == cameras.end() || !it->second.is_valid()) {
        return nullptr;
    }
    
    return it->second.get<Camera>();
}

Camera* CameraManager::getCameraByName(const std::string& name) {
    auto it = cameraNames.find(name);
    if (it == cameraNames.end()) {
        return nullptr;
    }
    return getCamera(it->second);
}

const Camera* CameraManager::getCameraByName(const std::string& name) const {
    auto it = cameraNames.find(name);
    if (it == cameraNames.end()) {
        return nullptr;
    }
    return getCamera(it->second);
}

flecs::entity CameraManager::getCameraEntity(CameraID cameraID) {
    if (cameraID == 0) {
        cameraID = activeCameraID;
    }
    
    auto it = cameras.find(cameraID);
    if (it != cameras.end()) {
        return it->second;
    }
    
    return flecs::entity::null();
}

void CameraManager::setActiveCamera(CameraID cameraID) {
    if (cameraID == 0 || cameras.find(cameraID) != cameras.end()) {
        activeCameraID = cameraID;
    }
}

Camera* CameraManager::getActiveCameraData() {
    return getCamera(activeCameraID);
}

const Camera* CameraManager::getActiveCameraData() const {
    return getCamera(activeCameraID);
}

void CameraManager::handleWindowResize(int width, int height) {
    windowSize = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    
    if (!initialized || !world) {
        return;
    }
    
    for (const auto& [cameraID, entity] : cameras) {
        if (entity.is_valid()) {
            Camera* camera = entity.get_mut<Camera>();
            if (camera) {
                float aspectRatio = windowSize.x / windowSize.y;
                camera->setAspectRatio(aspectRatio);
            }
        }
    }
}

std::vector<CameraID> CameraManager::getAllCameraIDs() const {
    std::vector<CameraID> ids;
    ids.reserve(cameras.size());
    
    for (const auto& [cameraID, entity] : cameras) {
        ids.push_back(cameraID);
    }
    
    return ids;
}

std::vector<std::string> CameraManager::getCameraNames() const {
    std::vector<std::string> names;
    names.reserve(cameraNames.size());
    
    for (const auto& [name, cameraID] : cameraNames) {
        names.push_back(name);
    }
    
    return names;
}

CameraID CameraManager::findNearestCamera(const glm::vec3& position) const {
    if (cameras.empty()) {
        return 0;
    }
    
    CameraID nearestID = 0;
    float minDistance = std::numeric_limits<float>::max();
    
    for (const auto& [cameraID, entity] : cameras) {
        if (!entity.is_valid()) continue;
        
        const Camera* camera = entity.get<Camera>();
        if (!camera) continue;
        
        glm::vec3 cameraPos = camera->position;
        float distance = glm::length(position - cameraPos);
        
        if (distance < minDistance) {
            minDistance = distance;
            nearestID = cameraID;
        }
    }
    
    return nearestID;
}

void CameraManager::moveCamera(CameraID cameraID, const glm::vec3& delta) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setPosition(camera->position + delta);
    }
}

void CameraManager::setCameraPosition(CameraID cameraID, const glm::vec3& position) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setPosition(position);
    }
}

void CameraManager::setCameraZoom(CameraID cameraID, float zoom) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setZoom(zoom);
    }
}

void CameraManager::setCameraRotation(CameraID cameraID, float rotation) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setRotation(rotation);
    }
}

void CameraManager::focusCameraOn(CameraID cameraID, const glm::vec3& target, float zoom) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setPosition(target);
        if (zoom > 0.0f) {
            camera->setZoom(zoom);
        }
    }
}

void CameraManager::createOrthographicCamera(const std::string& name, const glm::vec3& position, float zoom, const glm::vec2& viewSize) {
    Camera camera;
    camera.setPosition(position);
    camera.setZoom(zoom);
    camera.viewSize = viewSize;
    
    createCamera(camera, name);
}

void CameraManager::createFollowCamera(const std::string& name, const glm::vec3& target, float distance, float zoom) {
    Camera camera;
    glm::vec3 position = target + glm::vec3(0.0f, 0.0f, distance);
    camera.setPosition(position);
    camera.setZoom(zoom);
    
    createCamera(camera, name);
}

void CameraManager::printCameraInfo(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID == 0 ? activeCameraID : cameraID);
    if (!camera) {
        std::cout << "Camera " << cameraID << " not found\n";
        return;
    }
    
    std::cout << "Camera " << cameraID << ":\n";
    std::cout << "  Position: " << camera->position.x << ", " << camera->position.y << ", " << camera->position.z << "\n";
    std::cout << "  Zoom: " << camera->zoom << "\n";
    std::cout << "  Rotation: " << camera->rotation << "\n";
}

void CameraManager::printAllCameras() const {
    std::cout << "All cameras (" << cameras.size() << "):\n";
    for (const auto& [cameraID, entity] : cameras) {
        printCameraInfo(cameraID);
    }
}

void CameraManager::ensureMainCamera() {
    if (!cameras.empty()) {
        return;
    }
    
    CameraID mainID = createCamera(Camera{}, "main");
    if (mainID != 0) {
        activeCameraID = mainID;
    }
}

flecs::entity CameraManager::createCameraEntity(const Camera& cameraData, const std::string& name) {
    if (!world) {
        return flecs::entity::null();
    }
    
    std::string entityName = name.empty() ? "camera_" + std::to_string(nextCameraID) : name;
    
    flecs::entity entity = world->entity(entityName.c_str());
    entity.add<Camera>();
    
    Camera* camera = entity.get_mut<Camera>();
    if (camera) {
        *camera = cameraData;
        
        // Ensure matrices are marked dirty for proper initialization
        camera->viewDirty = true;
        camera->projectionDirty = true;
        
        float aspectRatio = windowSize.x / windowSize.y;
        camera->setAspectRatio(aspectRatio);
    }
    
    return entity;
}

flecs::entity CameraManager::createMainCameraEntity() {
    Camera mainCamera;
    return createCameraEntity(mainCamera, "main");
}