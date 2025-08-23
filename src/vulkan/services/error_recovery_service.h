#pragma once

#include <memory>
#include <flecs.h>

// Forward declarations
class PresentationSurface;
class RenderFrameDirector;

// Forward declaration - the actual type is in render_frame_director.h
struct RenderFrameResult;

class ErrorRecoveryService {
public:
    ErrorRecoveryService(PresentationSurface* presentationSurface);
    ~ErrorRecoveryService() = default;

    // Main error recovery entry point
    bool handleFrameFailure(const RenderFrameResult& frameResult, 
                           RenderFrameDirector* frameDirector, 
                           uint32_t currentFrame, 
                           float totalTime, 
                           float deltaTime, 
                           uint32_t frameCounter, 
                           flecs::world* world,
                           RenderFrameResult& retryResult);

private:
    PresentationSurface* presentationSurface = nullptr;

    // Error analysis
    bool shouldAttemptSwapchainRecreation() const;
    const char* determineRecreationReason() const;
    
    // Recovery operations
    bool attemptSwapchainRecreation();
    bool retryFrameAfterRecreation(RenderFrameDirector* frameDirector, 
                                  uint32_t currentFrame, 
                                  float totalTime, 
                                  float deltaTime, 
                                  uint32_t frameCounter, 
                                  flecs::world* world,
                                  RenderFrameResult& retryResult);
};