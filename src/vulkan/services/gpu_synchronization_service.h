#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include "../core/vulkan_constants.h"

// Forward declarations
class VulkanContext;

class GPUSynchronizationService {
public:
    GPUSynchronizationService();
    ~GPUSynchronizationService();

    bool initialize(const VulkanContext& context);
    void cleanup();

    // Fence management
    VkResult waitForComputeFence(uint32_t frameIndex, const char* fenceName = "compute");
    VkResult waitForGraphicsFence(uint32_t frameIndex, const char* fenceName = "graphics");
    
    VkFence getComputeFence(uint32_t frameIndex) const { return computeFences[frameIndex]; }
    VkFence getGraphicsFence(uint32_t frameIndex) const { return graphicsFences[frameIndex]; }
    
    bool isComputeInUse(uint32_t frameIndex) const { return computeInUse[frameIndex]; }
    bool isGraphicsInUse(uint32_t frameIndex) const { return graphicsInUse[frameIndex]; }
    void setComputeInUse(uint32_t frameIndex, bool inUse) { computeInUse[frameIndex] = inUse; }
    void setGraphicsInUse(uint32_t frameIndex, bool inUse) { graphicsInUse[frameIndex] = inUse; }

    // Wait for all in-use fences (for swapchain recreation)
    bool waitForAllFrames();

private:
    const VulkanContext* context = nullptr;
    bool initialized = false;

    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> computeFences{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> graphicsFences{};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> computeInUse{};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> graphicsInUse{};

    // Helper method for robust fence waiting
    VkResult waitForFenceRobust(VkFence fence, const char* fenceName);
};