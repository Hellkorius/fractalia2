#pragma once

#include <vulkan/vulkan.h>

// Forward declarations
class VulkanContext;
class VulkanSwapchain;
class VulkanPipeline;
class GPUSynchronizationService;

struct SurfaceAcquisitionResult {
    bool success = false;
    uint32_t imageIndex = 0;
    bool recreationNeeded = false;
    VkResult result = VK_SUCCESS;
};

class PresentationSurface {
public:
    PresentationSurface();
    ~PresentationSurface();

    bool initialize(
        VulkanContext* context,
        VulkanSwapchain* swapchain,
        VulkanPipeline* pipeline,
        GPUSynchronizationService* syncManager
    );
    
    void cleanup();

    // Main coordination methods
    SurfaceAcquisitionResult acquireNextImage(uint32_t currentFrame);
    bool recreateSwapchain();

    // Framebuffer resize handling
    void setFramebufferResized(bool resized) { framebufferResized = resized; }
    bool isFramebufferResized() const { return framebufferResized; }

private:
    // Dependencies
    VulkanContext* context = nullptr;
    VulkanSwapchain* swapchain = nullptr;
    VulkanPipeline* pipeline = nullptr;
    GPUSynchronizationService* syncManager = nullptr;

    // State tracking
    bool recreationInProgress = false;
    bool framebufferResized = false;
};