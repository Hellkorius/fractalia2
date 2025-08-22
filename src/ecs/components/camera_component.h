#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <iostream>

// Camera component for 3D view control
struct Camera {
    glm::vec3 position{0.0f, 0.0f, 10.0f};  // Camera position in world space
    glm::vec3 front{0.0f, 0.0f, -1.0f};     // Camera forward direction
    glm::vec3 up{0.0f, 1.0f, 0.0f};         // Camera up direction
    glm::vec3 right{1.0f, 0.0f, 0.0f};      // Camera right direction
    float yaw{-90.0f};                      // Yaw angle in degrees (left-right)
    float pitch{0.0f};                      // Pitch angle in degrees (up-down)
    float roll{0.0f};                       // Roll angle in degrees (tilt)
    float fov{45.0f};                       // Field of view in degrees
    float nearPlane{0.1f};                  // Near clipping plane
    float farPlane{1000.0f};                // Far clipping plane
    
    // Constructor to ensure proper initialization
    Camera() {
        // Initialize with default values already set above
        // Vectors will be properly calculated when initializeVectors() is called
        viewDirty = true;
        projectionDirty = true;
    }
    
    // Screen properties
    float aspectRatio{4.0f / 3.0f};        // Aspect ratio to maintain
    
    // Movement controls
    float moveSpeed{15.0f};                 // Units per second
    float mouseSensitivity{0.1f};           // Mouse sensitivity for look
    
    // Rotation limits
    float minPitch{-89.0f};                 // Minimum pitch angle (degrees)
    float maxPitch{89.0f};                  // Maximum pitch angle (degrees)
    
    // Cached matrices (updated when dirty)
    mutable glm::mat4 viewMatrix{1.0f};
    mutable glm::mat4 projectionMatrix{1.0f};
    mutable bool viewDirty{true};
    mutable bool projectionDirty{true};
    
    // Get view matrix (lazy computation)
    const glm::mat4& getViewMatrix() const {
        if (viewDirty) {
            // Create 3D view matrix using lookAt
            viewMatrix = glm::lookAt(position, position + front, up);
            viewDirty = false;
        }
        return viewMatrix;
    }
    
    // Get projection matrix (lazy computation)
    const glm::mat4& getProjectionMatrix() const {
        if (projectionDirty) {
            // Validate parameters before creating projection matrix
            float safeFov = glm::clamp(fov, 1.0f, 179.0f);
            float safeAspectRatio = aspectRatio > 0.0f ? aspectRatio : 1.0f;
            float safeNearPlane = nearPlane > 0.0f ? nearPlane : 0.1f;
            float safeFarPlane = farPlane > safeNearPlane ? farPlane : safeNearPlane + 1000.0f;
            
            // Create 3D perspective projection
            projectionMatrix = glm::perspective(glm::radians(safeFov), safeAspectRatio, safeNearPlane, safeFarPlane);
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
    
    void setFOV(float newFOV) {
        fov = glm::clamp(newFOV, 1.0f, 120.0f);
        projectionDirty = true;
    }
    
    void adjustFOV(float fovDelta) {
        setFOV(fov + fovDelta);
    }
    
    void setYaw(float newYaw) {
        yaw = newYaw;
        updateCameraVectors();
    }
    
    void setPitch(float newPitch) {
        pitch = glm::clamp(newPitch, minPitch, maxPitch);
        updateCameraVectors();
    }
    
    void setRoll(float newRoll) {
        roll = newRoll;
        updateCameraVectors();
    }
    
    // Initialize camera vectors on construction
    void initializeVectors() {
        updateCameraVectors();
    }
    
    void addYaw(float yawDelta) {
        setYaw(yaw + yawDelta);
    }
    
    void addPitch(float pitchDelta) {
        setPitch(pitch + pitchDelta);
    }
    
    void addRoll(float rollDelta) {
        setRoll(roll + rollDelta);
    }
    
private:
    void updateCameraVectors() {
        // Calculate front vector from yaw and pitch
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
        
        // Calculate right and up vectors
        right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        
        // Apply roll rotation
        if (abs(roll) > 0.001f) {
            glm::mat4 rollMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roll), front);
            up = glm::normalize(glm::vec3(rollMatrix * glm::vec4(glm::cross(right, front), 0.0f)));
            right = glm::normalize(glm::cross(front, up));
        } else {
            up = glm::normalize(glm::cross(right, front));
        }
        
        viewDirty = true;
    }
    
public:
    
    // Set aspect ratio and update projection
    void setAspectRatio(float ratio) {
        aspectRatio = ratio;
        projectionDirty = true;
    }
    
    // Convert screen coordinates to world coordinates (returns ray direction for 3D)
    glm::vec3 screenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
        if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) {
            return front; // Return camera front direction as fallback
        }
        
        // Normalize screen coordinates to [-1, 1]
        glm::vec2 normalized;
        normalized.x = (screenPos.x / screenSize.x) * 2.0f - 1.0f;
        normalized.y = (screenPos.y / screenSize.y) * 2.0f - 1.0f;
        
        // Apply inverse projection and view transformations
        glm::vec4 clipPos = glm::vec4(normalized, 1.0f, 1.0f);
        glm::mat4 projMatrix = getProjectionMatrix();
        glm::mat4 viewMatrix = getViewMatrix();
        
        // Check if matrices are valid
        if (projMatrix == glm::mat4(0.0f) || viewMatrix == glm::mat4(0.0f)) {
            return front; // Return camera front direction as fallback
        }
        
        glm::vec4 viewPos = glm::inverse(projMatrix) * clipPos;
        viewPos.w = 0.0f; // Direction vector
        
        glm::vec4 worldPos = glm::inverse(viewMatrix) * viewPos;
        glm::vec3 rayDir = glm::vec3(worldPos);
        
        // Ensure we return a valid normalized vector
        if (glm::length(rayDir) > 0.001f) {
            return glm::normalize(rayDir);
        } else {
            return front; // Return camera front direction as fallback
        }
    }
    
    // Get camera's forward direction
    const glm::vec3& getFront() const { return front; }
    const glm::vec3& getUp() const { return up; }
    const glm::vec3& getRight() const { return right; }
    
    // Move camera in local space
    void moveForward(float distance) {
        position += front * distance;
        viewDirty = true;
    }
    
    void moveBackward(float distance) {
        position -= front * distance;
        viewDirty = true;
    }
    
    void moveRight(float distance) {
        position += right * distance;
        viewDirty = true;
    }
    
    void moveLeft(float distance) {
        position -= right * distance;
        viewDirty = true;
    }
    
    void moveUp(float distance) {
        position += up * distance;
        viewDirty = true;
    }
    
    void moveDown(float distance) {
        position -= up * distance;
        viewDirty = true;
    }
    
    // Check if a world position is visible (basic frustum check)
    bool isVisible(const glm::vec3& worldPos) const {
        glm::vec3 toPoint = worldPos - position;
        float distance = glm::length(toPoint);
        
        // Check if within near/far planes
        if (distance < nearPlane || distance > farPlane) {
            return false;
        }
        
        // Check if within FOV cone (simplified)
        glm::vec3 direction = glm::normalize(toPoint);
        float angle = glm::degrees(acos(glm::dot(direction, front)));
        return angle <= fov * 0.5f;
    }
};