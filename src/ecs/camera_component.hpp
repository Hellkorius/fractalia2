#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Camera component for 2D view control
struct Camera {
    glm::vec3 position{0.0f, 0.0f, 0.0f};  // Camera position in world space
    float zoom{1.0f};                       // Zoom level (1.0 = normal, <1.0 = zoomed out, >1.0 = zoomed in)
    float rotation{0.0f};                   // Camera rotation in radians around Z-axis
    
    // View bounds (calculated from position/zoom)
    glm::vec2 viewSize{8.0f, 6.0f};        // Base view size (width, height)
    float aspectRatio{4.0f / 3.0f};        // Aspect ratio to maintain
    
    // Movement controls
    float moveSpeed{5.0f};                  // Units per second
    float zoomSpeed{2.0f};                  // Zoom factor per second
    float rotationSpeed{1.0f};              // Radians per second
    
    // Zoom limits
    float minZoom{0.1f};
    float maxZoom{10.0f};
    
    // Cached matrices (updated when dirty)
    mutable glm::mat4 viewMatrix{1.0f};
    mutable glm::mat4 projectionMatrix{1.0f};
    mutable bool viewDirty{true};
    mutable bool projectionDirty{true};
    
    // Get view matrix (lazy computation)
    const glm::mat4& getViewMatrix() const {
        if (viewDirty) {
            // Create view matrix: translate to camera position, then rotate
            viewMatrix = glm::rotate(glm::mat4(1.0f), -rotation, glm::vec3(0, 0, 1));
            viewMatrix = glm::translate(viewMatrix, -position);
            viewDirty = false;
        }
        return viewMatrix;
    }
    
    // Get projection matrix (lazy computation)
    const glm::mat4& getProjectionMatrix() const {
        if (projectionDirty) {
            // Calculate actual view size based on zoom
            float actualWidth = viewSize.x / zoom;
            float actualHeight = viewSize.y / zoom;
            
            // Create orthographic projection centered on camera
            float left = -actualWidth * 0.5f;
            float right = actualWidth * 0.5f;
            float bottom = -actualHeight * 0.5f;
            float top = actualHeight * 0.5f;
            
            projectionMatrix = glm::ortho(left, right, bottom, top, -5.0f, 5.0f);
            projectionMatrix[1][1] *= -1; // Flip Y for Vulkan
            projectionDirty = false;
        }
        return projectionMatrix;
    }
    
    // Movement functions
    void setPosition(const glm::vec3& pos) {
        position = pos;
        viewDirty = true;
    }
    
    void move(const glm::vec3& delta) {
        position += delta;
        viewDirty = true;
    }
    
    void setZoom(float newZoom) {
        zoom = glm::clamp(newZoom, minZoom, maxZoom);
        projectionDirty = true;
    }
    
    void adjustZoom(float zoomDelta) {
        setZoom(zoom * zoomDelta);
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
        normalized.y = 1.0f - (screenPos.y / screenSize.y) * 2.0f; // Flip Y
        
        // Apply inverse projection and view transformations
        glm::vec4 worldPos = glm::inverse(getProjectionMatrix() * getViewMatrix()) * glm::vec4(normalized, 0.0f, 1.0f);
        return glm::vec2(worldPos.x, worldPos.y);
    }
    
    // Check if a world position is visible
    bool isVisible(const glm::vec3& worldPos) const {
        float actualWidth = viewSize.x / zoom;
        float actualHeight = viewSize.y / zoom;
        
        return (worldPos.x >= position.x - actualWidth * 0.5f && 
                worldPos.x <= position.x + actualWidth * 0.5f &&
                worldPos.y >= position.y - actualHeight * 0.5f && 
                worldPos.y <= position.y + actualHeight * 0.5f);
    }
};