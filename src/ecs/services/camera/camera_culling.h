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
    int lodLevel = 0;
};

class CameraCulling {
public:
    CameraCulling() = default;
    ~CameraCulling() = default;

    std::vector<CullingInfo> performFrustumCulling(const std::vector<Transform>& transforms, 
                                                   const std::vector<Bounds>& bounds,
                                                   const Camera* camera) const;
    
    bool isEntityVisible(const Transform& transform, const Bounds& bounds, const Camera* camera) const;
    bool isPositionVisible(const glm::vec3& position, const Camera* camera) const;
    
    int calculateLODLevel(const glm::vec3& entityPosition, const Camera* camera) const;
    
    void setLODDistances(const std::vector<float>& distances) { lodDistances = distances; }
    const std::vector<float>& getLODDistances() const { return lodDistances; }
    
    struct CameraBounds {
        glm::vec2 min;
        glm::vec2 max;
        bool valid = false;
    };
    
    CameraBounds getCameraBounds(const Camera* camera) const;

private:
    std::vector<float> lodDistances = {10.0f, 50.0f, 100.0f, 500.0f};
    
    bool isInFrustum(const glm::vec3& position, const glm::vec3& bounds, const Camera* camera) const;
    float calculateDistanceToCamera(const glm::vec3& position, const Camera* camera) const;
};