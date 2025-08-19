#include "camera_service.h"
#include "../systems/camera_system.h"
#include <iostream>
#include <algorithm>
#include <cmath>

CameraService::CameraService() {
}

CameraService::~CameraService() {
    cleanup();
}

bool CameraService::initialize(flecs::world& world) {
    if (initialized) {
        return true;
    }
    
    this->world = &world;
    
    // Ensure main camera exists
    ensureMainCamera();
    
    initialized = true;
    return true;
}

void CameraService::cleanup() {
    if (!initialized) {
        return;
    }
    
    cameras.clear();
    cameraNames.clear();
    viewports.clear();
    
    activeCameraID = 0;
    nextCameraID = 1;
    currentTransition = CameraTransition{};
    
    world = nullptr;
    initialized = false;
}

void CameraService::update(float deltaTime) {
    if (!initialized) {
        return;
    }
    
    // Update camera transition
    updateTransition(deltaTime);
}

void CameraService::handleWindowResize(int width, int height) {
    windowSize = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    
    if (!initialized || !world) {
        return;
    }
    
    // Update aspect ratios for all cameras
    for (const auto& [cameraID, entity] : cameras) {
        if (entity.is_valid()) {
            Camera* camera = entity.get_mut<Camera>();
            if (camera) {
                float aspectRatio = windowSize.x / windowSize.y;
                camera->setAspectRatio(aspectRatio);
            }
        }
    }
    
    // Update existing camera manager
    CameraManager::updateAspectRatio(*world, width, height);
}

CameraID CameraService::createCamera(const std::string& name) {
    Camera defaultCamera;
    return createCamera(defaultCamera, name);
}

CameraID CameraService::createCamera(const Camera& cameraData, const std::string& name) {
    if (!initialized || !world) {
        return 0;
    }
    
    CameraID newID = nextCameraID++;
    
    // Create camera entity
    flecs::entity cameraEntity = createCameraEntity(cameraData, name);
    if (!cameraEntity.is_valid()) {
        return 0;
    }
    
    // Store camera mapping
    cameras[newID] = cameraEntity;
    
    // Store name mapping if provided
    if (!name.empty()) {
        cameraNames[name] = newID;
    }
    
    // Set as active if it's the first camera
    if (activeCameraID == 0) {
        activeCameraID = newID;
    }
    
    return newID;
}

bool CameraService::removeCamera(CameraID cameraID) {
    if (!initialized || cameraID == 0) {
        return false;
    }
    
    auto it = cameras.find(cameraID);
    if (it == cameras.end()) {
        return false;
    }
    
    // Remove from name mapping
    for (auto nameIt = cameraNames.begin(); nameIt != cameraNames.end(); ++nameIt) {
        if (nameIt->second == cameraID) {
            cameraNames.erase(nameIt);
            break;
        }
    }
    
    // Remove from viewports
    for (auto& [viewportName, viewport] : viewports) {
        if (viewport.cameraID == cameraID) {
            viewport.cameraID = 0; // Reset to main camera
        }
    }
    
    // Remove entity
    if (it->second.is_valid()) {
        it->second.destruct();
    }
    
    cameras.erase(it);
    
    // If this was the active camera, find another one
    if (activeCameraID == cameraID) {
        if (!cameras.empty()) {
            activeCameraID = cameras.begin()->first;
        } else {
            activeCameraID = 0;
            ensureMainCamera(); // Create new main camera
        }
    }
    
    return true;
}

bool CameraService::hasCamera(CameraID cameraID) const {
    return cameras.find(cameraID) != cameras.end();
}

Camera* CameraService::getCamera(CameraID cameraID) {
    if (cameraID == 0) {
        cameraID = activeCameraID;
    }
    
    auto it = cameras.find(cameraID);
    if (it != cameras.end() && it->second.is_valid()) {
        return it->second.get_mut<Camera>();
    }
    
    return nullptr;
}

const Camera* CameraService::getCamera(CameraID cameraID) const {
    if (cameraID == 0) {
        cameraID = activeCameraID;
    }
    
    auto it = cameras.find(cameraID);
    if (it != cameras.end() && it->second.is_valid()) {
        return it->second.get<Camera>();
    }
    
    return nullptr;
}

Camera* CameraService::getCameraByName(const std::string& name) {
    auto it = cameraNames.find(name);
    if (it != cameraNames.end()) {
        return getCamera(it->second);
    }
    return nullptr;
}

const Camera* CameraService::getCameraByName(const std::string& name) const {
    auto it = cameraNames.find(name);
    if (it != cameraNames.end()) {
        return getCamera(it->second);
    }
    return nullptr;
}

flecs::entity CameraService::getCameraEntity(CameraID cameraID) {
    if (cameraID == 0) {
        cameraID = activeCameraID;
    }
    
    auto it = cameras.find(cameraID);
    if (it != cameras.end()) {
        return it->second;
    }
    
    return flecs::entity::null();
}

void CameraService::setActiveCamera(CameraID cameraID) {
    if (hasCamera(cameraID)) {
        activeCameraID = cameraID;
    }
}

Camera* CameraService::getActiveCameraData() {
    return getCamera(activeCameraID);
}

const Camera* CameraService::getActiveCameraData() const {
    return getCamera(activeCameraID);
}

void CameraService::transitionToCamera(CameraID targetCameraID, const CameraTransition& transition) {
    if (!hasCamera(targetCameraID) || !initialized) {
        return;
    }
    
    const Camera* startCamera = getActiveCameraData();
    const Camera* endCamera = getCamera(targetCameraID);
    
    if (!startCamera || !endCamera) {
        return;
    }
    
    // Set up transition
    currentTransition = transition;
    currentTransition.startState = *startCamera;
    currentTransition.endState = *endCamera;
    currentTransition.currentTime = 0.0f;
    currentTransition.active = true;
    
    // The active camera will be updated during transition
}

void CameraService::transitionToCamera(CameraID targetCameraID, CameraTransitionType type, float duration) {
    CameraTransition transition;
    transition.type = type;
    transition.duration = duration;
    transitionToCamera(targetCameraID, transition);
}

void CameraService::transitionActiveCameraTo(const Camera& targetState, const CameraTransition& transition) {
    const Camera* startCamera = getActiveCameraData();
    if (!startCamera) {
        return;
    }
    
    // Set up transition
    currentTransition = transition;
    currentTransition.startState = *startCamera;
    currentTransition.endState = targetState;
    currentTransition.currentTime = 0.0f;
    currentTransition.active = true;
}

void CameraService::transitionActiveCameraTo(const Camera& targetState, CameraTransitionType type, float duration) {
    CameraTransition transition;
    transition.type = type;
    transition.duration = duration;
    transitionActiveCameraTo(targetState, transition);
}

void CameraService::cancelTransition() {
    currentTransition.active = false;
    currentTransition.currentTime = 0.0f;
}

void CameraService::createViewport(const std::string& name, CameraID cameraID, const glm::vec2& offset, const glm::vec2& size) {
    Viewport viewport;
    viewport.name = name;
    viewport.cameraID = cameraID;
    viewport.offset = offset;
    viewport.size = size;
    createViewport(viewport);
}

void CameraService::createViewport(const Viewport& viewport) {
    viewports[viewport.name] = viewport;
}

void CameraService::removeViewport(const std::string& name) {
    viewports.erase(name);
}

void CameraService::setViewportActive(const std::string& name, bool active) {
    auto it = viewports.find(name);
    if (it != viewports.end()) {
        it->second.active = active;
    }
}

bool CameraService::hasViewport(const std::string& name) const {
    return viewports.find(name) != viewports.end();
}

Viewport* CameraService::getViewport(const std::string& name) {
    auto it = viewports.find(name);
    return it != viewports.end() ? &it->second : nullptr;
}

const Viewport* CameraService::getViewport(const std::string& name) const {
    auto it = viewports.find(name);
    return it != viewports.end() ? &it->second : nullptr;
}

std::vector<Viewport*> CameraService::getActiveViewports() {
    std::vector<Viewport*> activeViewports;
    for (auto& [name, viewport] : viewports) {
        if (viewport.active) {
            activeViewports.push_back(&viewport);
        }
    }
    
    // Sort by render order
    std::sort(activeViewports.begin(), activeViewports.end(),
             [](const Viewport* a, const Viewport* b) {
                 return a->renderOrder < b->renderOrder;
             });
    
    return activeViewports;
}

std::vector<const Viewport*> CameraService::getActiveViewports() const {
    std::vector<const Viewport*> activeViewports;
    for (const auto& [name, viewport] : viewports) {
        if (viewport.active) {
            activeViewports.push_back(&viewport);
        }
    }
    
    // Sort by render order (const version)
    std::vector<const Viewport*> sortedViewports = activeViewports;
    std::sort(sortedViewports.begin(), sortedViewports.end(),
             [](const Viewport* a, const Viewport* b) {
                 return a->renderOrder < b->renderOrder;
             });
    
    return sortedViewports;
}

std::vector<CullingInfo> CameraService::performFrustumCulling(const std::vector<Transform>& transforms, 
                                                             const std::vector<Bounds>& bounds,
                                                             CameraID cameraID) const {
    std::vector<CullingInfo> cullingResults;
    const Camera* camera = getCamera(cameraID);
    
    if (!camera || transforms.size() != bounds.size()) {
        return cullingResults;
    }
    
    cullingResults.reserve(transforms.size());
    
    for (size_t i = 0; i < transforms.size(); ++i) {
        CullingInfo info;
        info.position = transforms[i].position;
        info.bounds = bounds[i].max - bounds[i].min; // Convert to half extents
        info.bounds *= 0.5f;
        
        info.visible = isInFrustum(info.position, info.bounds, *camera);
        info.distanceToCamera = calculateDistanceToCamera(info.position, *camera);
        info.lodLevel = calculateLODLevel(info.position, cameraID);
        
        cullingResults.push_back(info);
    }
    
    return cullingResults;
}

bool CameraService::isEntityVisible(const Transform& transform, const Bounds& bounds, CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera) {
        return false;
    }
    
    glm::vec3 halfExtents = (bounds.max - bounds.min) * 0.5f;
    return isInFrustum(transform.position, halfExtents, *camera);
}

int CameraService::calculateLODLevel(const glm::vec3& entityPosition, CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera) {
        return 0;
    }
    
    float distance = calculateDistanceToCamera(entityPosition, *camera);
    
    for (size_t i = 0; i < lodDistances.size(); ++i) {
        if (distance <= lodDistances[i]) {
            return static_cast<int>(i);
        }
    }
    
    return static_cast<int>(lodDistances.size());
}

glm::vec2 CameraService::worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize, CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera) {
        return glm::vec2(0.0f);
    }
    
    // Transform world position to clip space
    glm::vec4 worldPos4 = glm::vec4(worldPos, 1.0f);
    glm::vec4 clipPos = camera->getProjectionMatrix() * camera->getViewMatrix() * worldPos4;
    
    // Perspective divide
    if (std::abs(clipPos.w) < 1e-6f) {
        return glm::vec2(0.0f);
    }
    
    glm::vec2 ndcPos = glm::vec2(clipPos.x / clipPos.w, clipPos.y / clipPos.w);
    
    // Convert from NDC [-1, 1] to screen coordinates [0, screenSize]
    glm::vec2 screenPos;
    screenPos.x = (ndcPos.x + 1.0f) * 0.5f * screenSize.x;
    screenPos.y = (1.0f - ndcPos.y) * 0.5f * screenSize.y; // Flip Y
    
    return screenPos;
}

glm::vec2 CameraService::screenToWorld(const glm::vec2& screenPos, const glm::vec2& screenSize, CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera) {
        return glm::vec2(0.0f);
    }
    
    return camera->screenToWorld(screenPos, screenSize);
}

glm::vec2 CameraService::viewportToWorld(const glm::vec2& viewportPos, const std::string& viewportName) const {
    const Viewport* viewport = getViewport(viewportName);
    if (!viewport) {
        return glm::vec2(0.0f);
    }
    
    // Convert viewport coordinates to screen coordinates
    glm::vec2 screenPos = viewportPos;
    screenPos.x = viewport->offset.x * windowSize.x + viewportPos.x * viewport->size.x * windowSize.x;
    screenPos.y = viewport->offset.y * windowSize.y + viewportPos.y * viewport->size.y * windowSize.y;
    
    return screenToWorld(screenPos, windowSize, viewport->cameraID);
}

glm::vec3 CameraService::getCameraPosition(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? camera->position : glm::vec3(0.0f);
}

float CameraService::getCameraZoom(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? camera->zoom : 1.0f;
}

float CameraService::getCameraRotation(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? camera->rotation : 0.0f;
}

glm::mat4 CameraService::getViewMatrix(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? camera->getViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 CameraService::getProjectionMatrix(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? camera->getProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 CameraService::getViewProjectionMatrix(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? (camera->getProjectionMatrix() * camera->getViewMatrix()) : glm::mat4(1.0f);
}

CameraService::CameraBounds CameraService::getCameraBounds(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera) {
        return CameraBounds{};
    }
    
    float actualWidth = camera->viewSize.x / camera->zoom;
    float actualHeight = camera->viewSize.y / camera->zoom;
    
    CameraBounds bounds;
    bounds.min = glm::vec2(camera->position.x - actualWidth * 0.5f,
                          camera->position.y - actualHeight * 0.5f);
    bounds.max = glm::vec2(camera->position.x + actualWidth * 0.5f,
                          camera->position.y + actualHeight * 0.5f);
    bounds.valid = true;
    
    return bounds;
}

bool CameraService::isPositionVisible(const glm::vec3& position, CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    return camera ? camera->isVisible(position) : false;
}

std::vector<CameraID> CameraService::getAllCameraIDs() const {
    std::vector<CameraID> ids;
    ids.reserve(cameras.size());
    
    for (const auto& [id, entity] : cameras) {
        ids.push_back(id);
    }
    
    return ids;
}

std::vector<std::string> CameraService::getCameraNames() const {
    std::vector<std::string> names;
    names.reserve(cameraNames.size());
    
    for (const auto& [name, id] : cameraNames) {
        names.push_back(name);
    }
    
    return names;
}

CameraID CameraService::findNearestCamera(const glm::vec3& position) const {
    CameraID nearestID = 0;
    float nearestDistance = std::numeric_limits<float>::max();
    
    for (const auto& [id, entity] : cameras) {
        if (entity.is_valid()) {
            const Camera* camera = entity.get<Camera>();
            if (camera) {
                float distance = glm::length(camera->position - position);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearestID = id;
                }
            }
        }
    }
    
    return nearestID;
}

void CameraService::moveCamera(CameraID cameraID, const glm::vec3& delta) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->move(delta);
    }
}

void CameraService::setCameraPosition(CameraID cameraID, const glm::vec3& position) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setPosition(position);
    }
}

void CameraService::setCameraZoom(CameraID cameraID, float zoom) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setZoom(zoom);
    }
}

void CameraService::setCameraRotation(CameraID cameraID, float rotation) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setRotation(rotation);
    }
}

void CameraService::focusCameraOn(CameraID cameraID, const glm::vec3& target, float zoom) {
    Camera* camera = getCamera(cameraID);
    if (camera) {
        camera->setPosition(target);
        if (zoom > 0.0f) {
            camera->setZoom(zoom);
        }
    }
}

void CameraService::createOrthographicCamera(const std::string& name, const glm::vec3& position, float zoom, const glm::vec2& viewSize) {
    Camera camera;
    camera.position = position;
    camera.zoom = zoom;
    camera.viewSize = viewSize;
    camera.setAspectRatio(windowSize.x / windowSize.y);
    
    createCamera(camera, name);
}

void CameraService::createFollowCamera(const std::string& name, const glm::vec3& target, float distance, float zoom) {
    // For 2D cameras, "distance" doesn't apply in the same way as 3D
    // We'll interpret this as zoom level
    createOrthographicCamera(name, target, zoom, glm::vec2(200.0f, 150.0f));
}

void CameraService::printCameraInfo(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera) {
        std::cout << "Camera " << cameraID << " not found" << std::endl;
        return;
    }
    
    std::cout << "=== Camera " << cameraID << " Info ===" << std::endl;
    std::cout << "Position: (" << camera->position.x << ", " << camera->position.y << ", " << camera->position.z << ")" << std::endl;
    std::cout << "Zoom: " << camera->zoom << std::endl;
    std::cout << "Rotation: " << camera->rotation << " radians" << std::endl;
    std::cout << "View Size: " << camera->viewSize.x << " x " << camera->viewSize.y << std::endl;
    std::cout << "Aspect Ratio: " << camera->aspectRatio << std::endl;
    
    CameraBounds bounds = getCameraBounds(cameraID);
    if (bounds.valid) {
        std::cout << "Visible Bounds: [" << bounds.min.x << ", " << bounds.min.y << "] to [" << bounds.max.x << ", " << bounds.max.y << "]" << std::endl;
    }
}

void CameraService::printAllCameras() const {
    std::cout << "=== All Cameras ===" << std::endl;
    std::cout << "Active Camera ID: " << activeCameraID << std::endl;
    std::cout << "Total Cameras: " << cameras.size() << std::endl;
    
    for (const auto& [id, entity] : cameras) {
        printCameraInfo(id);
        std::cout << std::endl;
    }
}

void CameraService::printViewportInfo() const {
    std::cout << "=== Viewports ===" << std::endl;
    std::cout << "Total Viewports: " << viewports.size() << std::endl;
    
    for (const auto& [name, viewport] : viewports) {
        std::cout << "Viewport '" << name << "':" << std::endl;
        std::cout << "  Camera ID: " << viewport.cameraID << std::endl;
        std::cout << "  Offset: (" << viewport.offset.x << ", " << viewport.offset.y << ")" << std::endl;
        std::cout << "  Size: " << viewport.size.x << " x " << viewport.size.y << std::endl;
        std::cout << "  Active: " << (viewport.active ? "Yes" : "No") << std::endl;
        std::cout << "  Render Order: " << viewport.renderOrder << std::endl;
        std::cout << std::endl;
    }
}

// Private methods
void CameraService::updateTransition(float deltaTime) {
    if (!currentTransition.active) {
        return;
    }
    
    currentTransition.currentTime += deltaTime;
    float t = currentTransition.currentTime / currentTransition.duration;
    
    if (t >= 1.0f) {
        // Transition complete
        t = 1.0f;
        currentTransition.active = false;
        
        // Execute completion callback
        if (currentTransition.onComplete) {
            currentTransition.onComplete();
        }
    }
    
    // Apply easing
    float easedT = evaluateEasing(t, currentTransition.type);
    
    // Interpolate camera state
    Camera interpolatedCamera = interpolateCameras(currentTransition.startState, currentTransition.endState, easedT);
    
    // Apply to active camera
    Camera* activeCamera = getActiveCameraData();
    if (activeCamera) {
        *activeCamera = interpolatedCamera;
    }
}

float CameraService::evaluateEasing(float t, CameraTransitionType type) const {
    switch (type) {
        case CameraTransitionType::INSTANT:
            return 1.0f;
            
        case CameraTransitionType::LINEAR:
            return t;
            
        case CameraTransitionType::SMOOTH_STEP:
            return t * t * (3.0f - 2.0f * t);
            
        case CameraTransitionType::EASE_IN:
            return t * t;
            
        case CameraTransitionType::EASE_OUT:
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case CameraTransitionType::EASE_IN_OUT:
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
            }
            
        case CameraTransitionType::SPRING: {
            // Simple spring animation
            float omega = 10.0f; // Spring frequency
            float zeta = 0.8f;   // Damping ratio
            float exp_term = std::exp(-zeta * omega * t);
            float cos_term = std::cos(omega * std::sqrt(1.0f - zeta * zeta) * t);
            return 1.0f - exp_term * cos_term;
        }
        
        default:
            return t;
    }
}

Camera CameraService::interpolateCameras(const Camera& start, const Camera& end, float t) const {
    Camera result = start;
    
    // Interpolate position
    result.position = glm::mix(start.position, end.position, t);
    
    // Interpolate zoom
    result.zoom = glm::mix(start.zoom, end.zoom, t);
    
    // Interpolate rotation (handle wrapping)
    float rotationDiff = end.rotation - start.rotation;
    while (rotationDiff > glm::pi<float>()) rotationDiff -= 2.0f * glm::pi<float>();
    while (rotationDiff < -glm::pi<float>()) rotationDiff += 2.0f * glm::pi<float>();
    result.rotation = start.rotation + rotationDiff * t;
    
    // Interpolate view size
    result.viewSize = glm::mix(start.viewSize, end.viewSize, t);
    
    // Copy other properties from end state
    result.aspectRatio = end.aspectRatio;
    result.moveSpeed = end.moveSpeed;
    result.zoomSpeed = end.zoomSpeed;
    result.rotationSpeed = end.rotationSpeed;
    result.minZoom = end.minZoom;
    result.maxZoom = end.maxZoom;
    
    return result;
}

void CameraService::ensureMainCamera() {
    if (!initialized || !world) {
        return;
    }
    
    // Check if we already have a main camera from CameraManager
    flecs::entity mainCamera = CameraManager::getMainCamera(*world);
    if (mainCamera.is_valid()) {
        // Adopt the existing main camera
        CameraID mainID = nextCameraID++;
        cameras[mainID] = mainCamera;
        cameraNames["main"] = mainID;
        if (activeCameraID == 0) {
            activeCameraID = mainID;
        }
    } else {
        // Create a new main camera through CameraManager
        mainCamera = CameraManager::createMainCamera(*world);
        if (mainCamera.is_valid()) {
            CameraID mainID = nextCameraID++;
            cameras[mainID] = mainCamera;
            cameraNames["main"] = mainID;
            if (activeCameraID == 0) {
                activeCameraID = mainID;
            }
        }
    }
}

flecs::entity CameraService::createCameraEntity(const Camera& cameraData, const std::string& name) {
    if (!world) {
        return flecs::entity::null();
    }
    
    flecs::entity entity = world->entity();
    if (!name.empty()) {
        entity.set_name(name.c_str());
    }
    
    entity.set<Camera>(cameraData);
    
    return entity;
}

bool CameraService::isInFrustum(const glm::vec3& position, const glm::vec3& bounds, const Camera& camera) const {
    // For 2D orthographic cameras, frustum culling is essentially bounds checking
    glm::vec3 cameraMin = glm::vec3(
        camera.position.x - camera.viewSize.x / (2.0f * camera.zoom),
        camera.position.y - camera.viewSize.y / (2.0f * camera.zoom),
        camera.position.z - 5.0f // Near plane
    );
    
    glm::vec3 cameraMax = glm::vec3(
        camera.position.x + camera.viewSize.x / (2.0f * camera.zoom),
        camera.position.y + camera.viewSize.y / (2.0f * camera.zoom),
        camera.position.z + 5.0f // Far plane
    );
    
    glm::vec3 entityMin = position - bounds;
    glm::vec3 entityMax = position + bounds;
    
    // AABB intersection test
    return (entityMax.x >= cameraMin.x && entityMin.x <= cameraMax.x) &&
           (entityMax.y >= cameraMin.y && entityMin.y <= cameraMax.y) &&
           (entityMax.z >= cameraMin.z && entityMin.z <= cameraMax.z);
}

float CameraService::calculateDistanceToCamera(const glm::vec3& position, const Camera& camera) const {
    return glm::length(position - camera.position);
}

// Convenience namespace implementation
namespace CameraSystem {
    CameraService& getService() {
        return ServiceLocator::instance().requireService<CameraService>();
    }
    
    Camera* getActiveCamera() {
        return getService().getActiveCameraData();
    }
    
    const Camera* getActiveCameraConst() {
        return getService().getActiveCameraData();
    }
    
    CameraID getActiveCameraID() {
        return getService().getActiveCameraID();
    }
    
    void setActiveCamera(CameraID cameraID) {
        getService().setActiveCamera(cameraID);
    }
    
    glm::vec2 worldToScreen(const glm::vec3& worldPos) {
        // Use current window size - this might need to be updated based on actual window size
        return getService().worldToScreen(worldPos, glm::vec2(800.0f, 600.0f));
    }
    
    glm::vec2 screenToWorld(const glm::vec2& screenPos) {
        return getService().screenToWorld(screenPos, glm::vec2(800.0f, 600.0f));
    }
    
    bool isVisible(const glm::vec3& position) {
        return getService().isPositionVisible(position);
    }
    
    glm::vec3 getCameraPosition() {
        return getService().getCameraPosition();
    }
    
    float getCameraZoom() {
        return getService().getCameraZoom();
    }
    
    void moveCamera(const glm::vec3& delta) {
        getService().moveCamera(0, delta);
    }
    
    void setCameraPosition(const glm::vec3& position) {
        getService().setCameraPosition(0, position);
    }
    
    void setCameraZoom(float zoom) {
        getService().setCameraZoom(0, zoom);
    }
    
    void focusOn(const glm::vec3& target, float zoom) {
        getService().focusCameraOn(0, target, zoom);
    }
    
    void transitionTo(const glm::vec3& position, float zoom, float duration) {
        Camera targetState;
        targetState.position = position;
        targetState.zoom = zoom;
        getService().transitionActiveCameraTo(targetState, CameraTransitionType::SMOOTH_STEP, duration);
    }
    
    void transitionTo(CameraID cameraID, float duration) {
        getService().transitionToCamera(cameraID, CameraTransitionType::SMOOTH_STEP, duration);
    }
}