#include "camera_service.h"

CameraService::CameraService() = default;

CameraService::~CameraService() {
    cleanup();
}

bool CameraService::initialize(flecs::world& world) {
    if (initialized) {
        return true;
    }
    
    cameraManager = std::make_unique<CameraManager>();
    transitionSystem = std::make_unique<CameraTransitionSystem>();
    viewportManager = std::make_unique<ViewportManager>();
    culling = std::make_unique<CameraCulling>();
    transforms = std::make_unique<CameraTransforms>();
    
    if (!cameraManager->initialize(world)) {
        return false;
    }
    
    initialized = true;
    return true;
}

void CameraService::cleanup() {
    if (!initialized) {
        return;
    }
    
    cameraManager.reset();
    transitionSystem.reset();
    viewportManager.reset();
    culling.reset();
    transforms.reset();
    
    initialized = false;
}

void CameraService::update(float deltaTime) {
    if (!initialized) {
        return;
    }
    
    transitionSystem->update(deltaTime);
    updateActiveCamera();
}

void CameraService::handleWindowResize(int width, int height) {
    if (!initialized) {
        return;
    }
    
    glm::vec2 screenSize(width, height);
    
    cameraManager->handleWindowResize(width, height);
    transforms->setScreenSize(screenSize);
    viewportManager->setScreenSize(screenSize);
}

CameraID CameraService::createCamera(const std::string& name) {
    return initialized ? cameraManager->createCamera(name) : 0;
}

CameraID CameraService::createCamera(const Camera& cameraData, const std::string& name) {
    return initialized ? cameraManager->createCamera(cameraData, name) : 0;
}

bool CameraService::removeCamera(CameraID cameraID) {
    return initialized ? cameraManager->removeCamera(cameraID) : false;
}

bool CameraService::hasCamera(CameraID cameraID) const {
    return initialized ? cameraManager->hasCamera(cameraID) : false;
}

Camera* CameraService::getCamera(CameraID cameraID) {
    return initialized ? cameraManager->getCamera(cameraID) : nullptr;
}

const Camera* CameraService::getCamera(CameraID cameraID) const {
    return initialized ? cameraManager->getCamera(cameraID) : nullptr;
}

Camera* CameraService::getCameraByName(const std::string& name) {
    return initialized ? cameraManager->getCameraByName(name) : nullptr;
}

const Camera* CameraService::getCameraByName(const std::string& name) const {
    return initialized ? cameraManager->getCameraByName(name) : nullptr;
}

flecs::entity CameraService::getCameraEntity(CameraID cameraID) {
    return initialized ? cameraManager->getCameraEntity(cameraID) : flecs::entity::null();
}

void CameraService::setActiveCamera(CameraID cameraID) {
    if (initialized) {
        cameraManager->setActiveCamera(cameraID);
    }
}

CameraID CameraService::getActiveCameraID() const {
    return initialized ? cameraManager->getActiveCameraID() : 0;
}

Camera* CameraService::getActiveCameraData() {
    return initialized ? cameraManager->getActiveCameraData() : nullptr;
}

const Camera* CameraService::getActiveCameraData() const {
    return initialized ? cameraManager->getActiveCameraData() : nullptr;
}

void CameraService::transitionToCamera(CameraID targetCameraID, const CameraTransition& transition) {
    if (!initialized) return;
    
    const Camera* sourceCamera = getActiveCameraData();
    const Camera* targetCamera = getCamera(targetCameraID);
    
    transitionSystem->transitionToCamera(getActiveCameraID(), targetCameraID, sourceCamera, targetCamera, transition);
}

void CameraService::transitionToCamera(CameraID targetCameraID, CameraTransitionType type, float duration) {
    if (!initialized) return;
    
    const Camera* sourceCamera = getActiveCameraData();
    const Camera* targetCamera = getCamera(targetCameraID);
    
    transitionSystem->transitionToCamera(getActiveCameraID(), targetCameraID, sourceCamera, targetCamera, type, duration);
}

void CameraService::transitionActiveCameraTo(const Camera& targetState, const CameraTransition& transition) {
    if (!initialized) return;
    
    const Camera* currentCamera = getActiveCameraData();
    transitionSystem->transitionActiveCameraTo(currentCamera, targetState, transition);
}

void CameraService::transitionActiveCameraTo(const Camera& targetState, CameraTransitionType type, float duration) {
    if (!initialized) return;
    
    const Camera* currentCamera = getActiveCameraData();
    transitionSystem->transitionActiveCameraTo(currentCamera, targetState, type, duration);
}

bool CameraService::isTransitionActive() const {
    return initialized ? transitionSystem->isTransitionActive() : false;
}

void CameraService::cancelTransition() {
    if (initialized) {
        transitionSystem->cancelTransition();
    }
}

void CameraService::createViewport(const std::string& name, CameraID cameraID, const glm::vec2& offset, const glm::vec2& size) {
    if (initialized) {
        viewportManager->createViewport(name, cameraID, offset, size);
    }
}

void CameraService::createViewport(const Viewport& viewport) {
    if (initialized) {
        viewportManager->createViewport(viewport);
    }
}

void CameraService::removeViewport(const std::string& name) {
    if (initialized) {
        viewportManager->removeViewport(name);
    }
}

void CameraService::setViewportActive(const std::string& name, bool active) {
    if (initialized) {
        viewportManager->setViewportActive(name, active);
    }
}

bool CameraService::hasViewport(const std::string& name) const {
    return initialized ? viewportManager->hasViewport(name) : false;
}

Viewport* CameraService::getViewport(const std::string& name) {
    return initialized ? viewportManager->getViewport(name) : nullptr;
}

const Viewport* CameraService::getViewport(const std::string& name) const {
    return initialized ? viewportManager->getViewport(name) : nullptr;
}

std::vector<Viewport*> CameraService::getActiveViewports() {
    return initialized ? viewportManager->getActiveViewports() : std::vector<Viewport*>{};
}

std::vector<const Viewport*> CameraService::getActiveViewports() const {
    if (!initialized) {
        return std::vector<const Viewport*>{};
    }
    
    auto activeViewports = viewportManager->getActiveViewports();
    std::vector<const Viewport*> result;
    result.reserve(activeViewports.size());
    for (const auto* viewport : activeViewports) {
        result.push_back(viewport);
    }
    return result;
}

bool CameraService::isEntityVisible(const Transform& transform, const Bounds& bounds, CameraID cameraID) const {
    if (!initialized) return false;
    
    const Camera* camera = getCameraForOperations(cameraID);
    return culling->isEntityVisible(transform, bounds, camera);
}


glm::vec2 CameraService::worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize, CameraID cameraID) const {
    if (!initialized) return glm::vec2(0.0f);
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->worldToScreen(worldPos, camera);
}

glm::vec2 CameraService::screenToWorld(const glm::vec2& screenPos, const glm::vec2& screenSize, CameraID cameraID) const {
    if (!initialized) return glm::vec2(0.0f);
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->screenToWorld(screenPos, camera);
}

glm::vec2 CameraService::viewportToWorld(const glm::vec2& viewportPos, const std::string& viewportName) const {
    if (!initialized) return glm::vec2(0.0f);
    
    const Viewport* viewport = viewportManager->getViewport(viewportName);
    const Camera* camera = getCamera(viewport ? viewport->cameraID : 0);
    return transforms->viewportToWorld(viewportPos, viewport, camera);
}

glm::vec3 CameraService::getCameraPosition(CameraID cameraID) const {
    if (!initialized) return glm::vec3(0.0f);
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->getCameraPosition(camera);
}

float CameraService::getCameraZoom(CameraID cameraID) const {
    if (!initialized) return 1.0f;
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->getCameraZoom(camera);
}

float CameraService::getCameraRotation(CameraID cameraID) const {
    if (!initialized) return 0.0f;
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->getCameraRotation(camera);
}

glm::mat4 CameraService::getViewMatrix(CameraID cameraID) const {
    if (!initialized) return glm::mat4(1.0f);
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->getViewMatrix(camera);
}

glm::mat4 CameraService::getProjectionMatrix(CameraID cameraID) const {
    if (!initialized) return glm::mat4(1.0f);
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->getProjectionMatrix(camera);
}

glm::mat4 CameraService::getViewProjectionMatrix(CameraID cameraID) const {
    if (!initialized) return glm::mat4(1.0f);
    
    const Camera* camera = getCameraForOperations(cameraID);
    return transforms->getViewProjectionMatrix(camera);
}

CameraService::CameraBounds CameraService::getCameraBounds(CameraID cameraID) const {
    if (!initialized) return CameraBounds{};
    
    const Camera* camera = getCameraForOperations(cameraID);
    return culling->getCameraBounds(camera);
}

bool CameraService::isPositionVisible(const glm::vec3& position, CameraID cameraID) const {
    if (!initialized) return false;
    
    const Camera* camera = getCameraForOperations(cameraID);
    return culling->isPositionVisible(position, camera);
}

std::vector<CameraID> CameraService::getAllCameraIDs() const {
    return initialized ? cameraManager->getAllCameraIDs() : std::vector<CameraID>{};
}

std::vector<std::string> CameraService::getCameraNames() const {
    return initialized ? cameraManager->getCameraNames() : std::vector<std::string>{};
}

CameraID CameraService::findNearestCamera(const glm::vec3& position) const {
    return initialized ? cameraManager->findNearestCamera(position) : 0;
}

void CameraService::moveCamera(CameraID cameraID, const glm::vec3& delta) {
    if (initialized) {
        cameraManager->moveCamera(cameraID, delta);
    }
}

void CameraService::setCameraPosition(CameraID cameraID, const glm::vec3& position) {
    if (initialized) {
        cameraManager->setCameraPosition(cameraID, position);
    }
}

void CameraService::setCameraZoom(CameraID cameraID, float zoom) {
    if (initialized) {
        cameraManager->setCameraZoom(cameraID, zoom);
    }
}

void CameraService::setCameraRotation(CameraID cameraID, float rotation) {
    if (initialized) {
        cameraManager->setCameraRotation(cameraID, rotation);
    }
}

void CameraService::focusCameraOn(CameraID cameraID, const glm::vec3& target, float zoom) {
    if (initialized) {
        cameraManager->focusCameraOn(cameraID, target, zoom);
    }
}

void CameraService::createOrthographicCamera(const std::string& name, const glm::vec3& position, float zoom, const glm::vec2& viewSize) {
    if (initialized) {
        cameraManager->createOrthographicCamera(name, position, zoom, viewSize);
    }
}

void CameraService::createFollowCamera(const std::string& name, const glm::vec3& target, float distance, float zoom) {
    if (initialized) {
        cameraManager->createFollowCamera(name, target, distance, zoom);
    }
}

void CameraService::printCameraInfo(CameraID cameraID) const {
    if (initialized) {
        cameraManager->printCameraInfo(cameraID);
    }
}

void CameraService::printAllCameras() const {
    if (initialized) {
        cameraManager->printAllCameras();
    }
}

void CameraService::printViewportInfo() const {
    if (initialized) {
        viewportManager->printViewportInfo();
    }
}

void CameraService::setDefaultTransitionType(CameraTransitionType type) {
    if (initialized) {
        transitionSystem->setDefaultTransitionType(type);
    }
}

void CameraService::setDefaultTransitionDuration(float duration) {
    if (initialized) {
        transitionSystem->setDefaultTransitionDuration(duration);
    }
}

CameraTransitionType CameraService::getDefaultTransitionType() const {
    return initialized ? transitionSystem->getDefaultTransitionType() : CameraTransitionType::LINEAR;
}

float CameraService::getDefaultTransitionDuration() const {
    return initialized ? transitionSystem->getDefaultTransitionDuration() : 1.0f;
}

void CameraService::updateActiveCamera() {
    if (!transitionSystem->isTransitionActive()) {
        return;
    }
    
    Camera transitionState = transitionSystem->getCurrentTransitionState();
    Camera* activeCamera = getActiveCameraData();
    
    if (activeCamera) {
        *activeCamera = transitionState;
    }
}

const Camera* CameraService::getCameraForOperations(CameraID cameraID) const {
    const Camera* camera = getCamera(cameraID);
    if (!camera && transitionSystem->isTransitionActive()) {
        static Camera transitionCamera;
        transitionCamera = transitionSystem->getCurrentTransitionState();
        return &transitionCamera;
    }
    return camera;
}

Camera* CameraService::getCameraForOperations(CameraID cameraID) {
    return const_cast<Camera*>(static_cast<const CameraService*>(this)->getCameraForOperations(cameraID));
}

