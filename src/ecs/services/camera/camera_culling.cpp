#include "camera_culling.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/constants.hpp>


bool CameraCulling::isEntityVisible(const Transform& transform, const Bounds& bounds, const Camera* camera) const {
    // No culling - everything is visible
    return true;
}

bool CameraCulling::isPositionVisible(const glm::vec3& position, const Camera* camera) const {
    // No culling - everything is visible
    return true;
}


CameraCulling::CameraBounds CameraCulling::getCameraBounds(const Camera* camera) const {
    CameraBounds bounds;
    
    if (!camera) {
        return bounds;
    }
    
    // No culling - return invalid bounds
    bounds.valid = false;
    return bounds;
}

bool CameraCulling::isInFrustum(const glm::vec3& position, const glm::vec3& bounds, const Camera* camera) const {
    // No culling - everything is visible
    return true;
}

float CameraCulling::calculateDistanceToCamera(const glm::vec3& position, const Camera* camera) const {
    if (!camera) {
        return 0.0f;
    }
    
    glm::vec3 cameraPos = camera->position;
    return glm::length(position - cameraPos);
}