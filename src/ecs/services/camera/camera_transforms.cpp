#include "camera_transforms.h"
#include <glm/gtc/matrix_transform.hpp>

glm::vec2 CameraTransforms::worldToScreen(const glm::vec3& worldPos, const Camera* camera) const {
    if (!camera) {
        return glm::vec2(0.0f);
    }
    
    // Use 3D projection matrices for world to screen conversion
    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 projMatrix = camera->getProjectionMatrix();
    glm::mat4 mvp = projMatrix * viewMatrix;
    
    glm::vec4 worldPos4 = glm::vec4(worldPos, 1.0f);
    glm::vec4 clipPos = mvp * worldPos4;
    
    if (clipPos.w == 0.0f) {
        return glm::vec2(0.0f);
    }
    
    // Perspective divide
    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    
    // Convert NDC to screen coordinates
    glm::vec2 screenPos;
    screenPos.x = (ndc.x + 1.0f) * 0.5f * screenSize.x;
    screenPos.y = (1.0f - ndc.y) * 0.5f * screenSize.y; // Flip Y for screen coordinates
    
    return screenPos;
}

glm::vec2 CameraTransforms::screenToWorld(const glm::vec2& screenPos, const Camera* camera) const {
    if (!camera) {
        return glm::vec2(0.0f);
    }
    
    // Use camera's 3D ray casting for screen to world conversion
    return glm::vec2(camera->screenToWorldRay(screenPos, screenSize));
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
    // For 3D cameras, return FOV instead of zoom
    return camera ? camera->fov : 45.0f;
}

float CameraTransforms::getCameraRotation(const Camera* camera) const {
    // For 3D cameras, return yaw rotation
    return camera ? camera->yaw : 0.0f;
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