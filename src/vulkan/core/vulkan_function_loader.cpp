#include "vulkan_function_loader.h"
#include <SDL3/SDL_vulkan.h>
#include <iostream>

// Macros to eliminate repetitive function loading patterns
#define LOAD_INSTANCE_FUNCTION(funcName) \
    funcName = reinterpret_cast<PFN_##funcName>( \
        vkGetInstanceProcAddr(instance, #funcName))

#define LOAD_DEVICE_FUNCTION(funcName) \
    funcName = reinterpret_cast<PFN_##funcName>( \
        vkGetDeviceProcAddr(device, #funcName))

#define LOAD_PRE_INSTANCE_FUNCTION(funcName) \
    funcName = reinterpret_cast<PFN_##funcName>( \
        vkGetInstanceProcAddr(VK_NULL_HANDLE, #funcName))

VulkanFunctionLoader::VulkanFunctionLoader() {
}

VulkanFunctionLoader::~VulkanFunctionLoader() {
    cleanup();
}

bool VulkanFunctionLoader::initialize(SDL_Window* window) {
    // Get SDL's Vulkan instance proc addr function
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        SDL_Vulkan_GetVkGetInstanceProcAddr());
    
    if (!vkGetInstanceProcAddr) {
        std::cerr << "Failed to get vkGetInstanceProcAddr from SDL" << std::endl;
        return false;
    }
    
    if (!loadInstanceFunctions()) {
        std::cerr << "Failed to load instance functions" << std::endl;
        return false;
    }
    
    // Note: Device creation and device function loading will be handled
    // by the VulkanContext module that uses this function loader
    
    return true;
}

void VulkanFunctionLoader::cleanup() {
    // Cleanup is handled by individual modules that own the Vulkan objects
    // This loader only manages function pointers
}

bool VulkanFunctionLoader::loadInstanceFunctions() {
    loadCoreInstanceFunctions();
    
    return true;
}

void VulkanFunctionLoader::loadPostInstanceFunctions() {
    loadPhysicalDeviceFunctions();
    loadSurfaceFunctions();
}

bool VulkanFunctionLoader::loadPostDeviceFunctions() {
    return loadDeviceFunctions();
}

bool VulkanFunctionLoader::loadDeviceFunctions() {
    if (device == VK_NULL_HANDLE) {
        std::cerr << "Cannot load device functions: device not set" << std::endl;
        return false;
    }
    
    // Get device proc addr function first
    vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    
    if (!vkGetDeviceProcAddr) {
        std::cerr << "Failed to get vkGetDeviceProcAddr" << std::endl;
        return false;
    }
    
    loadDeviceManagementFunctions();
    loadMemoryFunctions();
    loadBufferFunctions();
    loadImageFunctions();
    loadSwapchainFunctions();
    loadPipelineFunctions();
    loadDescriptorFunctions();
    loadSynchronizationFunctions();
    loadCommandFunctions();
    loadRenderingFunctions();
    loadQueueFunctions();
    
    return true;
}

void VulkanFunctionLoader::loadCoreInstanceFunctions() {
    LOAD_PRE_INSTANCE_FUNCTION(vkCreateInstance);
    LOAD_PRE_INSTANCE_FUNCTION(vkEnumerateInstanceExtensionProperties);
}

void VulkanFunctionLoader::loadPhysicalDeviceFunctions() {
    LOAD_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);
    LOAD_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties);
    // Load vkCreateDevice here since it's needed before device creation
    LOAD_INSTANCE_FUNCTION(vkCreateDevice);
}

void VulkanFunctionLoader::loadSurfaceFunctions() {
    LOAD_INSTANCE_FUNCTION(vkDestroySurfaceKHR);
    LOAD_INSTANCE_FUNCTION(vkDestroyInstance);
    
    // Debug utils functions
    LOAD_INSTANCE_FUNCTION(vkCreateDebugUtilsMessengerEXT);
    LOAD_INSTANCE_FUNCTION(vkDestroyDebugUtilsMessengerEXT);
}

void VulkanFunctionLoader::loadDeviceManagementFunctions() {
    // vkCreateDevice is now loaded in loadPhysicalDeviceFunctions() since it's needed before device creation
    LOAD_DEVICE_FUNCTION(vkDestroyDevice);
    LOAD_DEVICE_FUNCTION(vkGetDeviceQueue);
    LOAD_DEVICE_FUNCTION(vkDeviceWaitIdle);
}

void VulkanFunctionLoader::loadMemoryFunctions() {
    LOAD_DEVICE_FUNCTION(vkAllocateMemory);
    LOAD_DEVICE_FUNCTION(vkFreeMemory);
    LOAD_DEVICE_FUNCTION(vkMapMemory);
    LOAD_DEVICE_FUNCTION(vkUnmapMemory);
}

void VulkanFunctionLoader::loadBufferFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateBuffer);
    LOAD_DEVICE_FUNCTION(vkDestroyBuffer);
    LOAD_DEVICE_FUNCTION(vkGetBufferMemoryRequirements);
    LOAD_DEVICE_FUNCTION(vkBindBufferMemory);
}

void VulkanFunctionLoader::loadImageFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateImage);
    LOAD_DEVICE_FUNCTION(vkDestroyImage);
    LOAD_DEVICE_FUNCTION(vkGetImageMemoryRequirements);
    LOAD_DEVICE_FUNCTION(vkBindImageMemory);
    LOAD_DEVICE_FUNCTION(vkCreateImageView);
    LOAD_DEVICE_FUNCTION(vkDestroyImageView);
}

void VulkanFunctionLoader::loadSwapchainFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateSwapchainKHR);
    LOAD_DEVICE_FUNCTION(vkDestroySwapchainKHR);
    LOAD_DEVICE_FUNCTION(vkGetSwapchainImagesKHR);
    LOAD_DEVICE_FUNCTION(vkAcquireNextImageKHR);
    LOAD_DEVICE_FUNCTION(vkQueuePresentKHR);
    
    // Load VK_EXT_swapchain_maintenance1 extension functions (optional)
    LOAD_DEVICE_FUNCTION(vkReleaseSwapchainImagesEXT);
}

void VulkanFunctionLoader::loadPipelineFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateRenderPass);
    LOAD_DEVICE_FUNCTION(vkDestroyRenderPass);
    LOAD_DEVICE_FUNCTION(vkCreateFramebuffer);
    LOAD_DEVICE_FUNCTION(vkDestroyFramebuffer);
    LOAD_DEVICE_FUNCTION(vkCreateShaderModule);
    LOAD_DEVICE_FUNCTION(vkDestroyShaderModule);
    LOAD_DEVICE_FUNCTION(vkCreatePipelineLayout);
    LOAD_DEVICE_FUNCTION(vkDestroyPipelineLayout);
    LOAD_DEVICE_FUNCTION(vkCreatePipelineCache);
    LOAD_DEVICE_FUNCTION(vkDestroyPipelineCache);
    LOAD_DEVICE_FUNCTION(vkCreateGraphicsPipelines);
    LOAD_DEVICE_FUNCTION(vkCreateComputePipelines);
    LOAD_DEVICE_FUNCTION(vkDestroyPipeline);
}

void VulkanFunctionLoader::loadDescriptorFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateDescriptorSetLayout);
    LOAD_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout);
    LOAD_DEVICE_FUNCTION(vkCreateDescriptorPool);
    LOAD_DEVICE_FUNCTION(vkDestroyDescriptorPool);
    LOAD_DEVICE_FUNCTION(vkResetDescriptorPool);
    LOAD_DEVICE_FUNCTION(vkAllocateDescriptorSets);
    LOAD_DEVICE_FUNCTION(vkUpdateDescriptorSets);
}

void VulkanFunctionLoader::loadSynchronizationFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateSemaphore);
    LOAD_DEVICE_FUNCTION(vkDestroySemaphore);
    LOAD_DEVICE_FUNCTION(vkCreateFence);
    LOAD_DEVICE_FUNCTION(vkDestroyFence);
    LOAD_DEVICE_FUNCTION(vkWaitForFences);
    LOAD_DEVICE_FUNCTION(vkResetFences);
    LOAD_DEVICE_FUNCTION(vkGetFenceStatus);
    LOAD_DEVICE_FUNCTION(vkCreateQueryPool);
    LOAD_DEVICE_FUNCTION(vkDestroyQueryPool);
    
    // Vulkan 1.3 functions
    LOAD_DEVICE_FUNCTION(vkCmdBeginRendering);
    LOAD_DEVICE_FUNCTION(vkCmdEndRendering);
    LOAD_DEVICE_FUNCTION(vkCmdPipelineBarrier2);
    LOAD_DEVICE_FUNCTION(vkQueueSubmit2);
}

void VulkanFunctionLoader::loadCommandFunctions() {
    LOAD_DEVICE_FUNCTION(vkCreateCommandPool);
    LOAD_DEVICE_FUNCTION(vkDestroyCommandPool);
    LOAD_DEVICE_FUNCTION(vkAllocateCommandBuffers);
    LOAD_DEVICE_FUNCTION(vkFreeCommandBuffers);
    LOAD_DEVICE_FUNCTION(vkResetCommandBuffer);
    LOAD_DEVICE_FUNCTION(vkResetCommandPool);
    LOAD_DEVICE_FUNCTION(vkBeginCommandBuffer);
    LOAD_DEVICE_FUNCTION(vkEndCommandBuffer);
}

void VulkanFunctionLoader::loadRenderingFunctions() {
    LOAD_DEVICE_FUNCTION(vkCmdBeginRenderPass);
    LOAD_DEVICE_FUNCTION(vkCmdEndRenderPass);
    LOAD_DEVICE_FUNCTION(vkCmdBindPipeline);
    LOAD_DEVICE_FUNCTION(vkCmdSetViewport);
    LOAD_DEVICE_FUNCTION(vkCmdSetScissor);
    LOAD_DEVICE_FUNCTION(vkCmdDraw);
    LOAD_DEVICE_FUNCTION(vkCmdDrawIndexed);
    LOAD_DEVICE_FUNCTION(vkCmdBindDescriptorSets);
    LOAD_DEVICE_FUNCTION(vkCmdBindVertexBuffers);
    LOAD_DEVICE_FUNCTION(vkCmdBindIndexBuffer);
    LOAD_DEVICE_FUNCTION(vkCmdDispatch);
    LOAD_DEVICE_FUNCTION(vkCmdDispatchIndirect);
    LOAD_DEVICE_FUNCTION(vkCmdPipelineBarrier);
    LOAD_DEVICE_FUNCTION(vkCmdPushConstants);
    LOAD_DEVICE_FUNCTION(vkCmdCopyBuffer);
    LOAD_DEVICE_FUNCTION(vkCmdCopyBufferToImage);
}

void VulkanFunctionLoader::loadQueueFunctions() {
    LOAD_DEVICE_FUNCTION(vkQueueSubmit);
    LOAD_DEVICE_FUNCTION(vkQueueWaitIdle);
}

// Clean up macros to avoid namespace pollution
#undef LOAD_INSTANCE_FUNCTION
#undef LOAD_DEVICE_FUNCTION
#undef LOAD_PRE_INSTANCE_FUNCTION