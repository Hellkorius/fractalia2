#include "presentation_surface.h"
#include "vulkan_context.h"
#include "vulkan_swapchain.h"
#include "vulkan_pipeline.h"
#include "vulkan_function_loader.h"
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
    VulkanPipeline* pipeline,
    GPUSynchronizationService* syncManager
) {
    this->context = context;
    this->swapchain = swapchain;
    this->pipeline = pipeline;
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

    // Wait for all frame fences to be signaled
    if (!syncManager->waitForAllFrames()) {
        recreationInProgress = false;
        return false;
    }

    // Recreate swapchain
    if (!swapchain->recreate(pipeline->getRenderPass())) {
        recreationInProgress = false;
        return false;
    }

    // Recreate pipeline for new swapchain format
    if (!pipeline->recreate(swapchain->getImageFormat())) {
        recreationInProgress = false;
        return false;
    }

    // Recreate framebuffers
    if (!swapchain->createFramebuffers(pipeline->getRenderPass())) {
        recreationInProgress = false;
        return false;
    }

    framebufferResized = false;
    recreationInProgress = false;
    return true;
}