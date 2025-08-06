#pragma once

#include "../component.hpp"
#include <SDL3/SDL.h>
#include <flecs.h>
#include <glm/glm.hpp>

// Camera control system - handles camera input and updates camera matrices
void camera_control_system(flecs::entity e, Camera& camera, const float dt);

// Camera matrix update system - ensures matrices are current for rendering
void camera_matrix_system(flecs::entity e, Camera& camera);

// Camera manager functions
namespace CameraManager {
    // Create a main camera entity
    flecs::entity createMainCamera(flecs::world& world);
    
    // Get the main camera (singleton)
    flecs::entity getMainCamera(flecs::world& world);
    
    // Update camera aspect ratio based on window size
    void updateAspectRatio(flecs::world& world, int windowWidth, int windowHeight);
    
    // Get camera matrices for rendering
    struct CameraMatrices {
        glm::mat4 view;
        glm::mat4 projection;
        bool valid;
    };
    
    CameraMatrices getCameraMatrices(flecs::world& world);
    
    // Helper functions for coordinate conversion
    glm::vec2 screenToWorld(flecs::world& world, const glm::vec2& screenPos, const glm::vec2& screenSize);
    glm::vec2 worldToScreen(flecs::world& world, const glm::vec3& worldPos, const glm::vec2& screenSize);
}

// Camera query functions for easy access
namespace CameraQuery {
    glm::vec3 getCameraPosition(flecs::world& world);
    float getCameraZoom(flecs::world& world);
    float getCameraRotation(flecs::world& world);
    
    bool isWorldPositionVisible(flecs::world& world, const glm::vec3& worldPos);
    
    // Get camera bounds in world space
    struct CameraBounds {
        glm::vec2 min;
        glm::vec2 max;
        bool valid;
    };
    
    CameraBounds getCameraBounds(flecs::world& world);
}