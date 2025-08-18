#include "presentation_surface.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_swapchain.h"
#include "../pipelines/pipeline_system_manager.h"
#include "../core/vulkan_function_loader.h"
#include "gpu_synchronization_service.h"
#include <iostream>

PresentationSurface::PresentationSurface() {
}

PresentationSurface::~PresentationSurface() {
    cleanup();
}

bool PresentationSurface::initialize(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    PipelineSystemManager* pipelineSystem,
    GPUSynchronizationService* syncManager
) {
    this->context = context;
    this->swapchain = swapchain;
    this->pipelineSystem = pipelineSystem;
    this->syncManager = syncManager;
    return true;
}

void PresentationSurface::cleanup() {
    // Dependencies are managed externally
}

SurfaceAcquisitionResult PresentationSurface::acquireNextImage(uint32_t currentFrame) {
    SurfaceAcquisitionResult result;

    // Check if recreation is needed due to framebuffer resize
    if (framebufferResized) {
        result.recreationNeeded = true;
        return result;
    }

    // Acquire next swapchain image
    VkResult acquireResult = context->getLoader().vkAcquireNextImageKHR(
        context->getDevice(),
        swapchain->getSwapchain(),
        UINT64_MAX,
        VK_NULL_HANDLE, // We'll handle semaphore synchronization externally
        VK_NULL_HANDLE,
        &result.imageIndex
    );

    result.result = acquireResult;

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        result.recreationNeeded = true;
        return result;
    } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        std::cerr << "PresentationSurface: Failed to acquire swap chain image: " << acquireResult << std::endl;
        return result;
    }

    result.success = true;
    return result;
}

bool PresentationSurface::recreateSwapchain() {
    // Prevent concurrent recreation attempts
    if (recreationInProgress) {
        std::cout << "PresentationSurface: Recreation already in progress, skipping..." << std::endl;
        return true;
    }
    recreationInProgress = true;

    std::cout << "PresentationSurface: CRITICAL FIX - Skipping redundant waitForAllFrames() call" << std::endl;
    std::cout << "PresentationSurface: GPU synchronization already handled by VulkanRenderer before this call" << std::endl;
    
    // CRITICAL FIX FOR SECOND RESIZE CRASH: Skip redundant fence wait
    // VulkanRenderer already called syncService->waitForAllFrames() and reset fences
    // Calling waitForAllFrames() again on already-reset fences causes undefined behavior/crash
    // if (!syncManager->waitForAllFrames()) {
    //     recreationInProgress = false;
    //     return false;
    // }

    // Recreate render pass for new swapchain format
    std::cout << "PresentationSurface: About to recreate render pass..." << std::endl;
    if (!pipelineSystem->recreateRenderPass(swapchain->getImageFormat())) {
        std::cout << "PresentationSurface: ERROR - Failed to recreate render pass" << std::endl;
        recreationInProgress = false;
        return false;
    }
    std::cout << "PresentationSurface: Render pass recreation completed successfully" << std::endl;
    
    // CRITICAL FIX FOR SECOND RESIZE CRASH: Recreate pipeline caches after render pass recreation
    // This addresses VkPipelineCache internal corruption that survives first resize but crashes on second
    std::cout << "PresentationSurface: About to recreate all pipeline caches..." << std::endl;
    if (!pipelineSystem->recreateAllPipelineCaches()) {
        std::cerr << "PresentationSurface: CRITICAL ERROR - Failed to recreate pipeline caches during swapchain recreation" << std::endl;
        std::cerr << "  This may cause pipeline creation failures or crashes in subsequent frames" << std::endl;
        // Continue anyway - this is better than failing entirely
    } else {
        std::cout << "PresentationSurface: All pipeline caches recreated successfully" << std::endl;
    }

    // Recreate swapchain
    std::cout << "PresentationSurface: About to recreate swapchain..." << std::endl;
    if (!swapchain->recreate(pipelineSystem->getCurrentRenderPass())) {
        std::cout << "PresentationSurface: ERROR - Failed to recreate swapchain" << std::endl;
        recreationInProgress = false;
        return false;
    }
    std::cout << "PresentationSurface: Swapchain recreation completed successfully" << std::endl;

    // Recreate framebuffers
    if (!swapchain->createFramebuffers(pipelineSystem->getCurrentRenderPass())) {
        recreationInProgress = false;
        return false;
    }

    framebufferResized = false;
    recreationInProgress = false;
    return true;
}