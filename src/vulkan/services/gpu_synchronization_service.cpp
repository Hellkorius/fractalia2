#include "gpu_synchronization_service.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include <iostream>

GPUSynchronizationService::GPUSynchronizationService() {
}

GPUSynchronizationService::~GPUSynchronizationService() {
    cleanup();
}

bool GPUSynchronizationService::initialize(const VulkanContext& context) {
    this->context = &context;
    
    // Create compute fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        if (context.getLoader().vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS) {
            std::cerr << "GPUSynchronizationService: Failed to create compute fence for frame " << i << std::endl;
            cleanup();
            return false;
        }
        
        if (context.getLoader().vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &graphicsFences[i]) != VK_SUCCESS) {
            std::cerr << "GPUSynchronizationService: Failed to create graphics fence for frame " << i << std::endl;
            cleanup();
            return false;
        }
        
        computeInUse[i] = false;
        graphicsInUse[i] = false;
    }
    
    initialized = true;
    return true;
}

void GPUSynchronizationService::cleanup() {
    if (!context || !initialized) return;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (computeFences[i] != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyFence(context->getDevice(), computeFences[i], nullptr);
            computeFences[i] = VK_NULL_HANDLE;
        }
        if (graphicsFences[i] != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyFence(context->getDevice(), graphicsFences[i], nullptr);
            graphicsFences[i] = VK_NULL_HANDLE;
        }
    }
    
    initialized = false;
}

VkResult GPUSynchronizationService::waitForComputeFence(uint32_t frameIndex, const char* fenceName) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || !computeInUse[frameIndex]) {
        return VK_SUCCESS;
    }
    
    VkResult result = waitForFenceRobust(computeFences[frameIndex], fenceName);
    if (result == VK_SUCCESS) {
        computeInUse[frameIndex] = false;
    }
    return result;
}

VkResult GPUSynchronizationService::waitForGraphicsFence(uint32_t frameIndex, const char* fenceName) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || !graphicsInUse[frameIndex]) {
        return VK_SUCCESS;
    }
    
    VkResult result = waitForFenceRobust(graphicsFences[frameIndex], fenceName);
    if (result == VK_SUCCESS) {
        graphicsInUse[frameIndex] = false;
    }
    return result;
}

bool GPUSynchronizationService::waitForAllFrames() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (computeInUse[i]) {
            VkResult result = waitForFenceRobust(computeFences[i], "compute");
            if (result == VK_ERROR_DEVICE_LOST) {
                return false;
            }
            computeInUse[i] = false;
        }
        if (graphicsInUse[i]) {
            VkResult result = waitForFenceRobust(graphicsFences[i], "graphics");
            if (result == VK_ERROR_DEVICE_LOST) {
                return false;
            }
            graphicsInUse[i] = false;
        }
    }
    return true;
}

VkResult GPUSynchronizationService::waitForFenceRobust(VkFence fence, const char* fenceName) {
    const uint64_t timeoutNs = 2000000000ULL; // 2 seconds
    
    VkResult result = context->getLoader().vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, timeoutNs);
    
    if (result == VK_TIMEOUT) {
        std::cerr << "GPUSynchronizationService: Warning: " << fenceName << " fence timeout, forcing synchronization" << std::endl;
        // Force immediate wait with device idle as last resort
        result = context->getLoader().vkDeviceWaitIdle(context->getDevice());
        if (result != VK_SUCCESS) {
            std::cerr << "GPUSynchronizationService: Critical: Failed to synchronize " << fenceName << " pipeline: " << result << std::endl;
        }
    }
    
    return result;
}