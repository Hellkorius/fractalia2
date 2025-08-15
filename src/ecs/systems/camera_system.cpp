#include "camera_system.h"
#include "input_system.h"
#include "../camera_component.h"
#include <iostream>

// Camera control system - processes input and updates camera
void camera_control_system(flecs::entity e, Camera& camera, const float dt) {
    flecs::world world = e.world();
    
    // Get input state for camera controls
    glm::vec3 moveDelta{0.0f};
    float zoomDelta = 1.0f;
    float rotationDelta = 0.0f;
    
    // WASD movement
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_W)) {
        moveDelta.y += camera.moveSpeed * dt;
    }
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_S)) {
        moveDelta.y -= camera.moveSpeed * dt;
    }
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_A)) {
        moveDelta.x -= camera.moveSpeed * dt;
    }
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_D)) {
        moveDelta.x += camera.moveSpeed * dt;
    }
    
    // Q/E for up/down movement
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_Q)) {
        moveDelta.z += camera.moveSpeed * dt;
    }
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_E)) {
        moveDelta.z -= camera.moveSpeed * dt;
    }
    
    // Mouse wheel for zoom
    glm::vec2 wheelDelta = InputQuery::getMouseWheelDelta(world);
    if (wheelDelta.y != 0.0f) {
        float zoomFactor = 1.0f + (camera.zoomSpeed * wheelDelta.y * dt);
        zoomDelta = zoomFactor;
    }
    
    // Shift for speed boost
    float speedMultiplier = 1.0f;
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_LSHIFT) || InputQuery::isKeyDown(world, SDL_SCANCODE_RSHIFT)) {
        speedMultiplier = 2.5f;
    }
    
    // Ctrl for precision mode
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_LCTRL) || InputQuery::isKeyDown(world, SDL_SCANCODE_RCTRL)) {
        speedMultiplier = 0.25f;
    }
    
    // R/T for rotation
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_R)) {
        rotationDelta += camera.rotationSpeed * dt;
    }
    if (InputQuery::isKeyDown(world, SDL_SCANCODE_T)) {
        rotationDelta -= camera.rotationSpeed * dt;
    }
    
    // Apply movement with speed multiplier
    if (glm::length(moveDelta) > 0.0f) {
        // Apply rotation to movement direction for camera-relative movement
        if (camera.rotation != 0.0f) {
            float cosRot = cos(camera.rotation);
            float sinRot = sin(camera.rotation);
            glm::vec3 rotatedMoveDelta;
            rotatedMoveDelta.x = moveDelta.x * cosRot - moveDelta.y * sinRot;
            rotatedMoveDelta.y = moveDelta.x * sinRot + moveDelta.y * cosRot;
            rotatedMoveDelta.z = moveDelta.z;
            moveDelta = rotatedMoveDelta;
        }
        camera.move(moveDelta * speedMultiplier);
    }
    
    // Apply zoom
    if (zoomDelta != 1.0f) {
        camera.adjustZoom(zoomDelta);
    }
    
    // Apply rotation
    if (rotationDelta != 0.0f) {
        camera.rotate(rotationDelta * speedMultiplier);
    }
    
    // Reset camera with SPACE key
    if (InputQuery::isKeyPressed(world, SDL_SCANCODE_SPACE)) {
        camera.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        camera.setZoom(1.0f);
        camera.setRotation(0.0f);
        std::cout << "Camera reset to origin" << std::endl;
    }
    
    // Debug: Print camera info on C key press
    if (InputQuery::isKeyPressed(world, SDL_SCANCODE_C)) {
        std::cout << "Camera - Position: (" << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << ")"
                  << " Zoom: " << camera.zoom
                  << " Rotation: " << glm::degrees(camera.rotation) << "°" << std::endl;
    }
}

// Camera matrix update system - ensures matrices are up to date
void camera_matrix_system(flecs::entity e, Camera& camera) {
    // Matrices are computed on-demand through the Camera component's getter methods
    // This system just ensures the camera component exists and is active
    // The actual matrix computation is lazy and happens in getViewMatrix() and getProjectionMatrix()
    (void)e; // Suppress unused parameter warning
    (void)camera; // The matrices update themselves when accessed
}

// Camera manager implementation
namespace CameraManager {
    
    flecs::entity createMainCamera(flecs::world& world) {
        auto camera = world.entity("MainCamera")
            .set<Camera>({})
            .add<KeyboardControlled>()
            .add<MouseControlled>();
            
        // Set initial camera properties
        auto* cam = camera.get_mut<Camera>();
        if (cam) {
            cam->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            cam->setZoom(1.0f);
            cam->setRotation(0.0f);
            cam->moveSpeed = 5.0f;
            cam->zoomSpeed = 2.0f;
            cam->rotationSpeed = 1.0f;
        }
        
        std::cout << "Main camera created" << std::endl;
        return camera;
    }
    
    flecs::entity getMainCamera(flecs::world& world) {
        return world.lookup("MainCamera");
    }
    
    void updateAspectRatio(flecs::world& world, int windowWidth, int windowHeight) {
        auto camera = getMainCamera(world);
        if (camera.is_valid()) {
            auto* cam = camera.get_mut<Camera>();
            if (cam && windowHeight > 0) {
                float aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
                cam->setAspectRatio(aspectRatio);
            }
        }
    }
    
    CameraMatrices getCameraMatrices(flecs::world& world) {
        auto camera = getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                return {
                    cam->getViewMatrix(),
                    cam->getProjectionMatrix(),
                    true
                };
            }
        }
        
        // Return identity matrices if no camera found
        return {
            glm::mat4(1.0f),
            glm::mat4(1.0f),
            false
        };
    }
    
    glm::vec2 screenToWorld(flecs::world& world, const glm::vec2& screenPos, const glm::vec2& screenSize) {
        auto camera = getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                return cam->screenToWorld(screenPos, screenSize);
            }
        }
        
        // Fallback: simple screen-to-world conversion assuming identity matrices
        glm::vec2 normalized;
        normalized.x = (screenPos.x / screenSize.x) * 2.0f - 1.0f;
        normalized.y = 1.0f - (screenPos.y / screenSize.y) * 2.0f;
        return normalized * 4.0f; // Approximate world scaling
    }
    
    glm::vec2 worldToScreen(flecs::world& world, const glm::vec3& worldPos, const glm::vec2& screenSize) {
        auto camera = getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                // Transform world position to clip space
                glm::vec4 clipPos = cam->getProjectionMatrix() * cam->getViewMatrix() * glm::vec4(worldPos, 1.0f);
                
                // Perspective divide (not needed for orthographic, but good practice)
                if (clipPos.w != 0.0f) {
                    clipPos /= clipPos.w;
                }
                
                // Convert from [-1,1] to screen coordinates
                glm::vec2 screenPos;
                screenPos.x = (clipPos.x + 1.0f) * 0.5f * screenSize.x;
                screenPos.y = (1.0f - clipPos.y) * 0.5f * screenSize.y; // Flip Y for screen coordinates
                return screenPos;
            }
        }
        
        // Fallback: simple world-to-screen conversion
        return glm::vec2(screenSize * 0.5f);
    }
}

// Camera query implementation
namespace CameraQuery {
    
    glm::vec3 getCameraPosition(flecs::world& world) {
        auto camera = CameraManager::getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                return cam->position;
            }
        }
        return glm::vec3(0.0f);
    }
    
    float getCameraZoom(flecs::world& world) {
        auto camera = CameraManager::getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                return cam->zoom;
            }
        }
        return 1.0f;
    }
    
    float getCameraRotation(flecs::world& world) {
        auto camera = CameraManager::getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                return cam->rotation;
            }
        }
        return 0.0f;
    }
    
    bool isWorldPositionVisible(flecs::world& world, const glm::vec3& worldPos) {
        auto camera = CameraManager::getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                return cam->isVisible(worldPos);
            }
        }
        return true; // Assume visible if no camera
    }
    
    CameraBounds getCameraBounds(flecs::world& world) {
        auto camera = CameraManager::getMainCamera(world);
        if (camera.is_valid()) {
            const auto* cam = camera.get<Camera>();
            if (cam) {
                float actualWidth = cam->viewSize.x / cam->zoom;
                float actualHeight = cam->viewSize.y / cam->zoom;
                
                return {
                    glm::vec2(cam->position.x - actualWidth * 0.5f, cam->position.y - actualHeight * 0.5f),
                    glm::vec2(cam->position.x + actualWidth * 0.5f, cam->position.y + actualHeight * 0.5f),
                    true
                };
            }
        }
        
        return {
            glm::vec2(-4.0f, -3.0f),
            glm::vec2(4.0f, 3.0f),
            false
        };
    }
}