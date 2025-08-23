#include "camera_transforms.h"
#include <glm/gtc/matrix_transform.hpp>

glm::vec2 CameraTransforms::worldToScreen(const glm::vec3& worldPos, const Camera* camera) const {
    if (!camera) {
        return glm::vec2(0.0f);
    }
    
    glm::vec3 cameraPos = camera->position;
    float zoom = camera->zoom;
    glm::vec2 viewSize = camera->viewSize;
    
    glm::vec2 worldOffset = glm::vec2(worldPos.x - cameraPos.x, worldPos.y - cameraPos.y);
    glm::vec2 scaledOffset = worldOffset * zoom;
    
    glm::vec2 screenPos;
    screenPos.x = (scaledOffset.x / viewSize.x + 0.5f) * screenSize.x;
    screenPos.y = (scaledOffset.y / viewSize.y + 0.5f) * screenSize.y;
    
    return screenPos;
}

glm::vec2 CameraTransforms::screenToWorld(const glm::vec2& screenPos, const Camera* camera) const {
    if (!camera) {
        return glm::vec2(0.0f);
    }
    
    glm::vec3 cameraPos = camera->position;
    float zoom = camera->zoom;
    glm::vec2 viewSize = camera->viewSize;
    
    if (zoom <= 0.0f) {
        return glm::vec2(cameraPos.x, cameraPos.y);
    }
    
    glm::vec2 normalizedScreen;
    normalizedScreen.x = (screenPos.x / screenSize.x - 0.5f);
    normalizedScreen.y = -(screenPos.y / screenSize.y - 0.5f);  // Flip Y axis: SDL screen coords (Y=0 top) to world coords (Y=0 center)
    
    glm::vec2 worldOffset = (normalizedScreen * viewSize) / zoom;
    
    return glm::vec2(cameraPos.x + worldOffset.x, cameraPos.y + worldOffset.y);
}

glm::vec2 CameraTransforms::viewportToWorld(const glm::vec2& viewportPos, const Viewport* viewport, const Camera* camera) const {
    if (!viewport || !camera) {
        return glm::vec2(0.0f);
    }
    
    glm::vec4 screenRect = viewport->getScreenRect(screenSize);
    glm::vec2 screenPos = glm::vec2(screenRect.x + viewportPos.x * screenRect.z,
                                   screenRect.y + viewportPos.y * screenRect.w);
    
    return screenToWorld(screenPos, camera);
}

glm::vec3 CameraTransforms::getCameraPosition(const Camera* camera) const {
    return camera ? camera->position : glm::vec3(0.0f);
}

float CameraTransforms::getCameraZoom(const Camera* camera) const {
    return camera ? camera->zoom : 1.0f;
}

float CameraTransforms::getCameraRotation(const Camera* camera) const {
    return camera ? camera->rotation : 0.0f;
}

glm::mat4 CameraTransforms::getViewMatrix(const Camera* camera) const {
    if (!camera) {
        return glm::mat4(1.0f);
    }
    
    return camera->getViewMatrix();
}

glm::mat4 CameraTransforms::getProjectionMatrix(const Camera* camera) const {
    if (!camera) {
        return glm::mat4(1.0f);
    }
    
    return camera->getProjectionMatrix();
}

glm::mat4 CameraTransforms::getViewProjectionMatrix(const Camera* camera) const {
    if (!camera) {
        return glm::mat4(1.0f);
    }
    
    return camera->getProjectionMatrix() * camera->getViewMatrix();
}