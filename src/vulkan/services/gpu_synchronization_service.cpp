#include "gpu_synchronization_service.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include "../core/vulkan_constants.h"
#include <iostream>
#include <stdexcept>

GPUSynchronizationService::GPUSynchronizationService(VulkanContext* context) 
: context(context) {
    if (!context) {
        throw std::runtime_error("GPUSynchronizationService: context cannot be null");
    }
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFence computeFenceHandle = VulkanUtils::createFence(device, vk, true);
        if (computeFenceHandle == VK_NULL_HANDLE) {
            throw std::runtime_error("GPUSynchronizationService: Failed to create compute fence for frame " + std::to_string(i));
        }
        computeFences[i] = vulkan_raii::make_fence(computeFenceHandle, context);
        
        VkFence graphicsFenceHandle = VulkanUtils::createFence(device, vk, true);
        if (graphicsFenceHandle == VK_NULL_HANDLE) {
            throw std::runtime_error("GPUSynchronizationService: Failed to create graphics fence for frame " + std::to_string(i));
        }
        graphicsFences[i] = vulkan_raii::make_fence(graphicsFenceHandle, context);
        
        computeInUse[i] = false;
        graphicsInUse[i] = false;
    }
}

GPUSynchronizationService::~GPUSynchronizationService() = default;

void GPUSynchronizationService::cleanupBeforeContextDestruction() {
    // Explicit cleanup before context destruction to ensure proper destruction order
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        computeFences[i].reset();
        graphicsFences[i].reset();
    }
}

VkResult GPUSynchronizationService::waitForComputeFence(uint32_t frameIndex, const char* fenceName) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || !computeInUse[frameIndex]) {
        return VK_SUCCESS;
    }
    
    VkResult result = waitForFenceRobust(computeFences[frameIndex].get(), fenceName);
    if (result == VK_SUCCESS) {
        computeInUse[frameIndex] = false;
    }
    return result;
}

VkResult GPUSynchronizationService::waitForGraphicsFence(uint32_t frameIndex, const char* fenceName) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || !graphicsInUse[frameIndex]) {
        return VK_SUCCESS;
    }
    
    VkResult result = waitForFenceRobust(graphicsFences[frameIndex].get(), fenceName);
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
            VkResult result = waitForFenceRobust(computeFences[i].get(), "compute");
            if (result == VK_ERROR_DEVICE_LOST || result == VK_TIMEOUT) {
                std::cerr << "GPUSynchronizationService: Failed to wait for compute fence " << i << ": " << result << std::endl;
                return false;
            }
            computeInUse[i] = false;
            std::cout << "GPUSynchronizationService: Compute fence " << i << " signaled successfully" << std::endl;
        }
        if (graphicsInUse[i]) {
            std::cout << "GPUSynchronizationService: Waiting for graphics fence " << i << std::endl;
            VkResult result = waitForFenceRobust(graphicsFences[i].get(), "graphics");
            if (result == VK_ERROR_DEVICE_LOST || result == VK_TIMEOUT) {
                std::cerr << "GPUSynchronizationService: Failed to wait for graphics fence " << i << ": " << result << std::endl;
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
        allFences.push_back(computeFences[i].get());
        allFences.push_back(graphicsFences[i].get());
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
    
    const uint64_t timeoutNs = FENCE_TIMEOUT_2_SECONDS;
    
    VkResult result = vk.vkWaitForFences(device, 1, &fence, VK_TRUE, timeoutNs);
    
    if (result == VK_TIMEOUT) {
        std::cerr << "GPUSynchronizationService: Critical: " << fenceName << " fence timeout after 2 seconds" << std::endl;
        std::cerr << "  This indicates a GPU hang or driver issue. Propagating timeout error." << std::endl;
        // Return timeout instead of forcing device idle - let caller handle recovery
        return VK_TIMEOUT;
    }
    
    return result;
}