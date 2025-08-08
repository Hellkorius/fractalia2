#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <iostream>

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
    
    // Zoom limits (disabled for unbounded zoom)
    float minZoom{0.0f};  // Disabled - allow infinite zoom out
    float maxZoom{std::numeric_limits<float>::max()};  // Disabled - allow infinite zoom in
    
    // Cached matrices (updated when dirty)
    mutable glm::mat4 viewMatrix{1.0f};
    mutable glm::mat4 projectionMatrix{1.0f};
    mutable bool viewDirty{true};
    mutable bool projectionDirty{true};
    
    // Get view matrix (lazy computation)
    const glm::mat4& getViewMatrix() const {
        if (viewDirty) {
            // Create view matrix: translate first, then rotate
            viewMatrix = glm::translate(glm::mat4(1.0f), -position);
            viewMatrix = glm::rotate(viewMatrix, -rotation, glm::vec3(0, 0, 1));
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
        // Ensure zoom is positive but allow unbounded zoom
        zoom = std::max(newZoom, std::numeric_limits<float>::epsilon());
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
        normalized.y = (screenPos.y / screenSize.y) * 2.0f - 1.0f;
        
        // Apply inverse projection and view transformations
        // First inverse projection (clip → view), then inverse view (view → world)
        glm::vec4 clipPos = glm::vec4(normalized, 0.0f, 1.0f);
        glm::vec4 viewPos = glm::inverse(getProjectionMatrix()) * clipPos;
        glm::vec4 worldPos = glm::inverse(getViewMatrix()) * viewPos;
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