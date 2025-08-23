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
    
    CameraBounds bounds = getCameraBounds(camera);
    if (!bounds.valid) {
        return true;
    }
    
    return position.x >= bounds.min.x && position.x <= bounds.max.x &&
           position.y >= bounds.min.y && position.y <= bounds.max.y;
}


CameraCulling::CameraBounds CameraCulling::getCameraBounds(const Camera* camera) const {
    CameraBounds bounds;
    
    if (!camera) {
        return bounds;
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
    
    CameraBounds cameraBounds = getCameraBounds(camera);
    if (!cameraBounds.valid) {
        return true;
    }
    
    glm::vec2 entityMin = glm::vec2(position.x - bounds.x, position.y - bounds.y);
    glm::vec2 entityMax = glm::vec2(position.x + bounds.x, position.y + bounds.y);
    
    bool intersects = !(entityMax.x < cameraBounds.min.x || entityMin.x > cameraBounds.max.x ||
                       entityMax.y < cameraBounds.min.y || entityMin.y > cameraBounds.max.y);
    
    return intersects;
}

float CameraCulling::calculateDistanceToCamera(const glm::vec3& position, const Camera* camera) const {
    if (!camera) {
        return 0.0f;
    }
    
    glm::vec3 cameraPos = camera->position;
    return glm::length(position - cameraPos);
}