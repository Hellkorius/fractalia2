#include "camera_transition_system.h"
#include <cmath>
#include <algorithm>

void CameraTransitionSystem::update(float deltaTime) {
    updateTransition(deltaTime);
}

void CameraTransitionSystem::transitionToCamera(CameraID sourceCameraID, CameraID targetCameraID, 
                                               const Camera* sourceCamera, const Camera* targetCamera,
                                               const CameraTransition& transition) {
    if (!sourceCamera || !targetCamera) {
        return;
    }

    currentTransition = transition;
    currentTransition.startState = *sourceCamera;
    currentTransition.endState = *targetCamera;
    currentTransition.currentTime = 0.0f;
    currentTransition.active = true;
}

void CameraTransitionSystem::transitionToCamera(CameraID sourceCameraID, CameraID targetCameraID,
                                               const Camera* sourceCamera, const Camera* targetCamera,
                                               CameraTransitionType type, float duration) {
    CameraTransition transition;
    transition.type = type;
    transition.duration = duration;
    
    transitionToCamera(sourceCameraID, targetCameraID, sourceCamera, targetCamera, transition);
}

void CameraTransitionSystem::transitionActiveCameraTo(const Camera* currentCamera, const Camera& targetState, 
                                                     const CameraTransition& transition) {
    if (!currentCamera) {
        return;
    }

    currentTransition = transition;
    currentTransition.startState = *currentCamera;
    currentTransition.endState = targetState;
    currentTransition.currentTime = 0.0f;
    currentTransition.active = true;
}

void CameraTransitionSystem::transitionActiveCameraTo(const Camera* currentCamera, const Camera& targetState,
                                                     CameraTransitionType type, float duration) {
    CameraTransition transition;
    transition.type = type;
    transition.duration = duration;
    
    transitionActiveCameraTo(currentCamera, targetState, transition);
}

void CameraTransitionSystem::cancelTransition() {
    currentTransition.active = false;
    currentTransition.currentTime = 0.0f;
}

Camera CameraTransitionSystem::getCurrentTransitionState() const {
    if (!currentTransition.active) {
        return currentTransition.endState;
    }
    
    float normalizedTime = std::clamp(currentTransition.currentTime / currentTransition.duration, 0.0f, 1.0f);
    float easedTime = evaluateEasing(normalizedTime, currentTransition.type);
    
    return interpolateCameras(currentTransition.startState, currentTransition.endState, easedTime);
}

bool CameraTransitionSystem::hasValidTransition() const {
    return currentTransition.active && currentTransition.duration > 0.0f;
}

void CameraTransitionSystem::updateTransition(float deltaTime) {
    if (!currentTransition.active) {
        return;
    }
    
    currentTransition.currentTime += deltaTime;
    
    if (currentTransition.currentTime >= currentTransition.duration) {
        currentTransition.active = false;
        
        if (currentTransition.onComplete) {
            currentTransition.onComplete();
        }
    }
}

float CameraTransitionSystem::evaluateEasing(float t, CameraTransitionType type) const {
    t = std::clamp(t, 0.0f, 1.0f);
    
    switch (type) {
        case CameraTransitionType::INSTANT:
            return 1.0f;
            
        case CameraTransitionType::LINEAR:
            return t;
            
        case CameraTransitionType::SMOOTH_STEP:
            return t * t * (3.0f - 2.0f * t);
            
        case CameraTransitionType::EASE_IN:
            return t * t;
            
        case CameraTransitionType::EASE_OUT:
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case CameraTransitionType::EASE_IN_OUT:
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
            }
            
        case CameraTransitionType::SPRING: {
            const float springFactor = 0.1f;
            float springEase = 1.0f - std::exp(-6.0f * t) * std::cos(12.0f * t) * springFactor;
            return std::clamp(springEase, 0.0f, 1.0f);
        }
            
        default:
            return t;
    }
}

Camera CameraTransitionSystem::interpolateCameras(const Camera& start, const Camera& end, float t) const {
    Camera result;
    
    // Interpolate position
    glm::vec3 startPos = start.position;
    glm::vec3 endPos = end.position;
    result.setPosition(glm::mix(startPos, endPos, t));
    
    // Interpolate FOV instead of zoom
    float startFOV = start.fov;
    float endFOV = end.fov;
    result.setFOV(glm::mix(startFOV, endFOV, t));
    
    // Interpolate yaw angle
    float startYaw = start.yaw;
    float endYaw = end.yaw;
    
    float angleDiff = endYaw - startYaw;
    while (angleDiff > 180.0f) angleDiff -= 360.0f;
    while (angleDiff < -180.0f) angleDiff += 360.0f;
    
    result.setYaw(startYaw + angleDiff * t);
    
    // Interpolate pitch angle
    float startPitch = start.pitch;
    float endPitch = end.pitch;
    result.setPitch(glm::mix(startPitch, endPitch, t));
    
    // Interpolate roll angle
    float startRoll = start.roll;
    float endRoll = end.roll;
    result.setRoll(glm::mix(startRoll, endRoll, t));
    
    // Interpolate aspect ratio
    result.aspectRatio = glm::mix(start.aspectRatio, end.aspectRatio, t);
    
    return result;
}