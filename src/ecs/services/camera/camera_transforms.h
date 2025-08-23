#pragma once

#include "../../components/camera_component.h"
#include "viewport_manager.h"
#include <glm/glm.hpp>

class CameraTransforms {
public:
    CameraTransforms() = default;
    ~CameraTransforms() = default;

    void setScreenSize(const glm::vec2& size) { screenSize = size; }
    glm::vec2 getScreenSize() const { return screenSize; }

    glm::vec2 worldToScreen(const glm::vec3& worldPos, const Camera* camera) const;
    glm::vec2 screenToWorld(const glm::vec2& screenPos, const Camera* camera) const;
    glm::vec2 viewportToWorld(const glm::vec2& viewportPos, const Viewport* viewport, const Camera* camera) const;
    
    glm::vec3 getCameraPosition(const Camera* camera) const;
    float getCameraZoom(const Camera* camera) const;
    float getCameraRotation(const Camera* camera) const;
    
    glm::mat4 getViewMatrix(const Camera* camera) const;
    glm::mat4 getProjectionMatrix(const Camera* camera) const;
    glm::mat4 getViewProjectionMatrix(const Camera* camera) const;

private:
    glm::vec2 screenSize{800.0f, 600.0f};
};