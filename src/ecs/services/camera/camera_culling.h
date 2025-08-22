#pragma once

#include "../../components/component.h"
#include "../../components/camera_component.h"
#include <glm/glm.hpp>
#include <vector>

struct CullingInfo {
    glm::vec3 position;
    glm::vec3 bounds;
    bool visible = true;
    float distanceToCamera = 0.0f;
};

class CameraCulling {
public:
    CameraCulling() = default;
    ~CameraCulling() = default;

    bool isEntityVisible(const Transform& transform, const Bounds& bounds, const Camera* camera) const;
    bool isPositionVisible(const glm::vec3& position, const Camera* camera) const;
    
    struct CameraBounds {
        glm::vec2 min;
        glm::vec2 max;
        bool valid = false;
    };
    
    CameraBounds getCameraBounds(const Camera* camera) const;

private:
    bool isInFrustum(const glm::vec3& position, const glm::vec3& bounds, const Camera* camera) const;
    float calculateDistanceToCamera(const glm::vec3& position, const Camera* camera) const;
};