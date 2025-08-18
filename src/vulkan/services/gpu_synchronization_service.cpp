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
    
    // Cache loader and device references for performance
    const auto& vk = context.getLoader();
    const VkDevice device = context.getDevice();
    
    // Create compute fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        if (vk.vkCreateFence(device, &fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS) {
            std::cerr << "GPUSynchronizationService: Failed to create compute fence for frame " << i << std::endl;
            cleanup();
            return false;
        }
        
        if (vk.vkCreateFence(device, &fenceInfo, nullptr, &graphicsFences[i]) != VK_SUCCESS) {
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
    
    // Cache loader and device references for performance
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (computeFences[i] != VK_NULL_HANDLE) {
            vk.vkDestroyFence(device, computeFences[i], nullptr);
            computeFences[i] = VK_NULL_HANDLE;
        }
        if (graphicsFences[i] != VK_NULL_HANDLE) {
            vk.vkDestroyFence(device, graphicsFences[i], nullptr);
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
    std::cout << "GPUSynchronizationService: CRITICAL - Waiting for all frames before swapchain recreation" << std::endl;
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (computeInUse[i]) {
            std::cout << "GPUSynchronizationService: Waiting for compute fence " << i << std::endl;
            VkResult result = waitForFenceRobust(computeFences[i], "compute");
            if (result == VK_ERROR_DEVICE_LOST) {
                return false;
            }
            computeInUse[i] = false;
            std::cout << "GPUSynchronizationService: Compute fence " << i << " signaled successfully" << std::endl;
        }
        if (graphicsInUse[i]) {
            std::cout << "GPUSynchronizationService: Waiting for graphics fence " << i << std::endl;
            VkResult result = waitForFenceRobust(graphicsFences[i], "graphics");
            if (result == VK_ERROR_DEVICE_LOST) {
                return false;
            }
            graphicsInUse[i] = false;
            std::cout << "GPUSynchronizationService: Graphics fence " << i << " signaled successfully" << std::endl;
        }
    }
    
    // CRITICAL FIX FOR SECOND RESIZE CRASH: Reset all fences after waiting
    // This prevents fence timeline corruption that can survive first resize but crash on second
    std::cout << "GPUSynchronizationService: CRITICAL FIX - Resetting all fences after swapchain recreation wait" << std::endl;
    std::vector<VkFence> allFences;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        allFences.push_back(computeFences[i]);
        allFences.push_back(graphicsFences[i]);
    }
    
    // Cache loader and device references for performance
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    VkResult resetResult = vk.vkResetFences(device, 
                                           static_cast<uint32_t>(allFences.size()), 
                                           allFences.data());
    if (resetResult != VK_SUCCESS) {
        std::cerr << "GPUSynchronizationService: WARNING - Failed to reset fences after swapchain recreation: " << resetResult << std::endl;
        std::cerr << "  This may cause fence synchronization corruption in subsequent frames" << std::endl;
    } else {
        std::cout << "GPUSynchronizationService: All fences successfully reset after swapchain recreation" << std::endl;
    }
    
    return true;
}

VkResult GPUSynchronizationService::waitForFenceRobust(VkFence fence, const char* fenceName) {
    // Cache loader and device references for performance
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    const uint64_t timeoutNs = 2000000000ULL; // 2 seconds
    
    VkResult result = vk.vkWaitForFences(device, 1, &fence, VK_TRUE, timeoutNs);
    
    if (result == VK_TIMEOUT) {
        std::cerr << "GPUSynchronizationService: Warning: " << fenceName << " fence timeout, forcing synchronization" << std::endl;
        // Force immediate wait with device idle as last resort
        result = vk.vkDeviceWaitIdle(device);
        if (result != VK_SUCCESS) {
            std::cerr << "GPUSynchronizationService: Critical: Failed to synchronize " << fenceName << " pipeline: " << result << std::endl;
        }
    }
    
    return result;
}