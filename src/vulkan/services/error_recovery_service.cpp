#include "error_recovery_service.h"
#include "../core/vulkan_swapchain.h"
#include "presentation_surface.h"
#include "render_frame_director.h"
#include <iostream>
#include <stdexcept>

ErrorRecoveryService::ErrorRecoveryService(PresentationSurface* presentationSurface) 
: presentationSurface(presentationSurface) {
    if (!presentationSurface) {
        throw std::runtime_error("ErrorRecoveryService: presentationSurface cannot be null");
    }
}

bool ErrorRecoveryService::handleFrameFailure(const RenderFrameResult& frameResult, 
                                             RenderFrameDirector* frameDirector, 
                                             uint32_t currentFrame, 
                                             float totalTime, 
                                             float deltaTime, 
                                             uint32_t frameCounter, 
                                             flecs::world* world,
                                             RenderFrameResult& retryResult) {
    
    std::cerr << "ErrorRecoveryService: Frame " << frameCounter << " FAILED in frameDirector->directFrame()" << std::endl;
    
    if (!shouldAttemptSwapchainRecreation()) {
        std::cerr << "ErrorRecoveryService: Frame failure not suitable for swapchain recreation" << std::endl;
        return false;
    }

    const char* recreationReason = determineRecreationReason();
    std::cout << "ErrorRecoveryService: Initiating proactive swapchain recreation due to: " << recreationReason << std::endl;
    
    if (!attemptSwapchainRecreation()) {
        std::cerr << "ErrorRecoveryService: Swapchain recreation failed" << std::endl;
        return false;
    }

    std::cout << "ErrorRecoveryService: Swapchain recreation successful, retrying frame" << std::endl;
    
    return retryFrameAfterRecreation(frameDirector, currentFrame, totalTime, deltaTime, frameCounter, world, retryResult);
}

bool ErrorRecoveryService::shouldAttemptSwapchainRecreation() const {
    if (!presentationSurface) return false;
    
    // Check for framebuffer resize or other conditions that warrant recreation
    if (presentationSurface->isFramebufferResized()) {
        return true;
    }
    
    // For now, attempt recreation on any frame failure as a fallback
    return true;
}

const char* ErrorRecoveryService::determineRecreationReason() const {
    if (presentationSurface && presentationSurface->isFramebufferResized()) {
        return "framebuffer resize detected";
    }
    
    return "general frame failure (proactive recovery)";
}

bool ErrorRecoveryService::attemptSwapchainRecreation() {
    if (!presentationSurface) return false;
    
    return presentationSurface->recreateSwapchain();
}

bool ErrorRecoveryService::retryFrameAfterRecreation(RenderFrameDirector* frameDirector, 
                                                    uint32_t currentFrame, 
                                                    float totalTime, 
                                                    float deltaTime, 
                                                    uint32_t frameCounter, 
                                                    flecs::world* world,
                                                    RenderFrameResult& retryResult) {
    
    if (!frameDirector) {
        std::cerr << "ErrorRecoveryService: Frame director unavailable for retry" << std::endl;
        return false;
    }

    retryResult = frameDirector->directFrame(currentFrame, totalTime, deltaTime, frameCounter, world);
    
    if (retryResult.success) {
        std::cout << "ErrorRecoveryService: Frame retry after swapchain recreation succeeded" << std::endl;
        return true;
    } else {
        std::cerr << "ErrorRecoveryService: Frame retry after swapchain recreation still failed" << std::endl;
        return false;
    }
}