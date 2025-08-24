#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <iostream>

// Camera component for 3D view control
struct Camera {
    glm::vec3 position{0.0f, 0.0f, 5.0f};  // Camera position in world space (moved back for 3D)
    glm::vec3 target{0.0f, 0.0f, 0.0f};    // Camera target/look-at point
    glm::vec3 up{0.0f, 1.0f, 0.0f};        // Camera up vector
    
    // Projection parameters
    float fov{45.0f};                       // Field of view in degrees (for perspective)
    float aspectRatio{4.0f / 3.0f};        // Aspect ratio to maintain
    float nearPlane{0.1f};                  // Near clipping plane
    float farPlane{1000.0f};                // Far clipping plane
    
    // Legacy orthographic parameters (for backward compatibility)
    float zoom{1.0f};                       // Zoom level (for orthographic mode)
    float rotation{0.0f};                   // Camera rotation around Z-axis (for 2D compatibility)
    glm::vec2 viewSize{200.0f, 150.0f};    // Orthographic view size
    
    // Camera mode
    enum class ProjectionType { Perspective, Orthographic } projectionType{ProjectionType::Perspective};
    
    // Movement controls
    float moveSpeed{5.0f};                  // Units per second
    float zoomSpeed{2.0f};                  // Zoom factor per second
    float rotationSpeed{1.0f};              // Radians per second
    
    // Zoom limits (disabled for unbounded zoom)
    float minZoom{0.0f};  // Disabled - allow infinite zoom out
    float maxZoom{std::numeric_limits<float>::max()};  // Disabled - allow infinite zoom in
    
    // Cached matrices (updated when dirty)
    mutable glm::mat4 viewMatrix{1.0f};
    mutable glm::mat4 projectionMatrix{1.0f};
    mutable bool viewDirty{true};
    mutable bool projectionDirty{true};
    
    // Get view matrix (lazy computation) - 3D lookAt implementation
    const glm::mat4& getViewMatrix() const {
        if (viewDirty) {
            if (projectionType == ProjectionType::Perspective) {
                // 3D lookAt matrix
                viewMatrix = glm::lookAt(position, target, up);
            } else {
                // Legacy 2D view matrix (for backward compatibility)
                viewMatrix = glm::translate(glm::mat4(1.0f), -position);
                viewMatrix = glm::rotate(viewMatrix, -rotation, glm::vec3(0, 0, 1));
            }
            viewDirty = false;
        }
        return viewMatrix;
    }
    
    // Get projection matrix (lazy computation) - supports both perspective and orthographic
    const glm::mat4& getProjectionMatrix() const {
        if (projectionDirty) {
            if (projectionType == ProjectionType::Perspective) {
                // 3D perspective projection
                projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
                projectionMatrix[1][1] *= -1; // Flip Y for Vulkan
            } else {
                // Legacy orthographic projection (for backward compatibility)
                float actualWidth = viewSize.x / zoom;
                float actualHeight = viewSize.y / zoom;
                
                float left = -actualWidth * 0.5f;
                float right = actualWidth * 0.5f;
                float bottom = -actualHeight * 0.5f;
                float top = actualHeight * 0.5f;
                
                projectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
                projectionMatrix[1][1] *= -1; // Flip Y for Vulkan
            }
            projectionDirty = false;
        }
        return projectionMatrix;
    }
    
    // Movement functions - 3D camera controls
    void setPosition(const glm::vec3& pos) {
        position = pos;
        viewDirty = true;
    }
    
    void setTarget(const glm::vec3& tgt) {
        target = tgt;
        viewDirty = true;
    }
    
    void setUp(const glm::vec3& upVec) {
        up = glm::normalize(upVec);
        viewDirty = true;
    }
    
    void move(const glm::vec3& delta) {
        position += delta;
        if (projectionType == ProjectionType::Perspective) {
            target += delta; // Move target with camera to maintain look direction
        }
        viewDirty = true;
    }
    
    // 3D camera movement helpers
    void moveForward(float distance) {
        glm::vec3 forward = glm::normalize(target - position);
        position += forward * distance;
        target += forward * distance;
        viewDirty = true;
    }
    
    void moveRight(float distance) {
        glm::vec3 forward = glm::normalize(target - position);
        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        position += right * distance;
        target += right * distance;
        viewDirty = true;
    }
    
    void moveUp(float distance) {
        position += up * distance;
        target += up * distance;
        viewDirty = true;
    }
    
    // Look-at controls
    void lookAt(const glm::vec3& point) {
        target = point;
        viewDirty = true;
    }
    
    void orbit(const glm::vec3& center, float horizontalAngle, float verticalAngle, float radius) {
        position.x = center.x + radius * cos(verticalAngle) * cos(horizontalAngle);
        position.y = center.y + radius * sin(verticalAngle);
        position.z = center.z + radius * cos(verticalAngle) * sin(horizontalAngle);
        target = center;
        viewDirty = true;
    }
    
    void setZoom(float newZoom) {
        // Ensure zoom is positive but allow unbounded zoom
        zoom = std::max(newZoom, std::numeric_limits<float>::epsilon());
        projectionDirty = true;
    }
    
    void adjustZoom(float zoomDelta) {
        setZoom(zoom * zoomDelta);
    }
    
    // 3D projection controls
    void setFOV(float newFov) {
        fov = glm::clamp(newFov, 1.0f, 179.0f);
        projectionDirty = true;
    }
    
    void setNearFar(float nearVal, float farVal) {
        nearPlane = nearVal;
        farPlane = farVal;
        projectionDirty = true;
    }
    
    void setProjectionType(ProjectionType type) {
        projectionType = type;
        viewDirty = true;
        projectionDirty = true;
    }
    
    void setRotation(float newRotation) {
        rotation = newRotation;
        viewDirty = true;
    }
    
    void rotate(float rotationDelta) {
        rotation += rotationDelta;
        viewDirty = true;
    }
    
    // Set aspect ratio and update projection
    void setAspectRatio(float ratio) {
        aspectRatio = ratio;
        viewSize.y = viewSize.x / ratio;
        projectionDirty = true;
    }
    
    // Convert screen coordinates to world coordinates
    glm::vec2 screenToWorld(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
        // Normalize screen coordinates to [-1, 1]
        glm::vec2 normalized;
        normalized.x = (screenPos.x / screenSize.x) * 2.0f - 1.0f;
        normalized.y = (screenPos.y / screenSize.y) * 2.0f - 1.0f;
        
        // Apply inverse projection and view transformations
        // First inverse projection (clip → view), then inverse view (view → world)
        glm::vec4 clipPos = glm::vec4(normalized, 0.0f, 1.0f);
        glm::vec4 viewPos = glm::inverse(getProjectionMatrix()) * clipPos;
        glm::vec4 worldPos = glm::inverse(getViewMatrix()) * viewPos;
        return glm::vec2(worldPos.x, worldPos.y);
    }
    
    // Check if a world position is visible - 3D frustum culling
    bool isVisible(const glm::vec3& worldPos) const {
        if (projectionType == ProjectionType::Perspective) {
            // 3D frustum culling (simplified sphere test for now)
            glm::vec3 toPoint = worldPos - position;
            float distance = glm::length(toPoint);
            
            // Check if within near/far planes
            if (distance < nearPlane || distance > farPlane) {
                return false;
            }
            
            // Simple angle-based culling (more sophisticated frustum culling can be added later)
            glm::vec3 forward = glm::normalize(target - position);
            glm::vec3 toPointNorm = glm::normalize(toPoint);
            float angle = glm::degrees(acos(glm::dot(forward, toPointNorm)));
            
            return angle < (fov * 0.6f); // Rough approximation
        } else {
            // Legacy 2D orthographic visibility check
            float actualWidth = viewSize.x / zoom;
            float actualHeight = viewSize.y / zoom;
            
            return (worldPos.x >= position.x - actualWidth * 0.5f && 
                    worldPos.x <= position.x + actualWidth * 0.5f &&
                    worldPos.y >= position.y - actualHeight * 0.5f && 
                    worldPos.y <= position.y + actualHeight * 0.5f);
        }
    }
};