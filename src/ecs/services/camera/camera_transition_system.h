#pragma once

#include "../../components/camera_component.h"
#include <functional>

using CameraID = uint32_t;

enum class CameraTransitionType {
    INSTANT,
    LINEAR,
    SMOOTH_STEP,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    SPRING
};

struct CameraTransition {
    CameraTransitionType type = CameraTransitionType::INSTANT;
    float duration = 1.0f;
    float currentTime = 0.0f;
    bool active = false;
    
    Camera startState;
    Camera endState;
    
    std::function<float(float)> customEasing;
    std::function<void()> onComplete;
};

class CameraTransitionSystem {
public:
    CameraTransitionSystem() = default;
    ~CameraTransitionSystem() = default;

    void update(float deltaTime);

    void transitionToCamera(CameraID sourceCameraID, CameraID targetCameraID, 
                           const Camera* sourceCamera, const Camera* targetCamera,
                           const CameraTransition& transition);
    void transitionToCamera(CameraID sourceCameraID, CameraID targetCameraID,
                           const Camera* sourceCamera, const Camera* targetCamera,
                           CameraTransitionType type, float duration);
    
    void transitionActiveCameraTo(const Camera* currentCamera, const Camera& targetState, 
                                 const CameraTransition& transition);
    void transitionActiveCameraTo(const Camera* currentCamera, const Camera& targetState,
                                 CameraTransitionType type, float duration);

    bool isTransitionActive() const { return currentTransition.active; }
    void cancelTransition();
    
    Camera getCurrentTransitionState() const;
    bool hasValidTransition() const;

    void setDefaultTransitionType(CameraTransitionType type) { defaultTransitionType = type; }
    void setDefaultTransitionDuration(float duration) { defaultTransitionDuration = duration; }
    CameraTransitionType getDefaultTransitionType() const { return defaultTransitionType; }
    float getDefaultTransitionDuration() const { return defaultTransitionDuration; }

private:
    CameraTransition currentTransition;
    CameraTransitionType defaultTransitionType = CameraTransitionType::LINEAR;
    float defaultTransitionDuration = 1.0f;

    void updateTransition(float deltaTime);
    float evaluateEasing(float t, CameraTransitionType type) const;
    Camera interpolateCameras(const Camera& start, const Camera& end, float t) const;
};