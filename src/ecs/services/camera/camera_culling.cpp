#include "camera_culling.h"
#include <cmath>
#include <algorithm>


bool CameraCulling::isEntityVisible(const Transform& transform, const Bounds& bounds, const Camera* camera) const {
    if (!camera) {
        return false;
    }
    
    glm::vec3 size = bounds.max - bounds.min;
    return isInFrustum(transform.position, size * 0.5f, camera);
}

bool CameraCulling::isPositionVisible(const glm::vec3& position, const Camera* camera) const {
    if (!camera) {
        return false;
    }
    
    // Use the camera's own isVisible method which supports both orthographic and perspective
    return camera->isVisible(position);
}


CameraCulling::CameraBounds CameraCulling::getCameraBounds(const Camera* camera) const {
    CameraBounds bounds;
    
    if (!camera) {
        return bounds;
    }
    
    // Only calculate 2D bounds for orthographic cameras
    // Perspective cameras use proper 3D frustum culling via camera->isVisible()
    if (camera->projectionType != Camera::ProjectionType::Orthographic) {
        return bounds; // Invalid bounds for perspective cameras
    }
    
    glm::vec3 cameraPos = camera->position;
    float zoom = camera->zoom;
    glm::vec2 viewSize = camera->viewSize;
    
    if (zoom <= 0.0f || viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
        return bounds;
    }
    
    float halfWidth = (viewSize.x / zoom) * 0.5f;
    float halfHeight = (viewSize.y / zoom) * 0.5f;
    
    bounds.min = glm::vec2(cameraPos.x - halfWidth, cameraPos.y - halfHeight);
    bounds.max = glm::vec2(cameraPos.x + halfWidth, cameraPos.y + halfHeight);
    bounds.valid = true;
    
    return bounds;
}

bool CameraCulling::isInFrustum(const glm::vec3& position, const glm::vec3& bounds, const Camera* camera) const {
    if (!camera) {
        return false;
    }
    
    // For 3D frustum culling, delegate to camera's isVisible method
    // The bounds parameter represents the entity's extent from center
    // We check the center position for simplicity (more sophisticated algorithms can check all corners)
    return camera->isVisible(position);
}

float CameraCulling::calculateDistanceToCamera(const glm::vec3& position, const Camera* camera) const {
    if (!camera) {
        return 0.0f;
    }
    
    glm::vec3 cameraPos = camera->position;
    return glm::length(position - cameraPos);
}