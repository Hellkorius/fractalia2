#include "vulkan_function_loader.h"
#include <SDL3/SDL_vulkan.h>
#include <iostream>

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
    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
}

void VulkanFunctionLoader::loadPhysicalDeviceFunctions() {
    vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
    vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
    // Load vkCreateDevice here since it's needed before device creation
    vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        vkGetInstanceProcAddr(instance, "vkCreateDevice"));
}

void VulkanFunctionLoader::loadSurfaceFunctions() {
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
}

void VulkanFunctionLoader::loadDeviceManagementFunctions() {
    // vkCreateDevice is now loaded in loadPhysicalDeviceFunctions() since it's needed before device creation
    vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
        vkGetDeviceProcAddr(device, "vkDestroyDevice"));
    vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));
    vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(
        vkGetDeviceProcAddr(device, "vkDeviceWaitIdle"));
}

void VulkanFunctionLoader::loadMemoryFunctions() {
    vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
        vkGetDeviceProcAddr(device, "vkAllocateMemory"));
    vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
        vkGetDeviceProcAddr(device, "vkFreeMemory"));
    vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(
        vkGetDeviceProcAddr(device, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(
        vkGetDeviceProcAddr(device, "vkUnmapMemory"));
}

void VulkanFunctionLoader::loadBufferFunctions() {
    vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(
        vkGetDeviceProcAddr(device, "vkCreateBuffer"));
    vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(
        vkGetDeviceProcAddr(device, "vkDestroyBuffer"));
    vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
        vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements"));
    vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(
        vkGetDeviceProcAddr(device, "vkBindBufferMemory"));
}

void VulkanFunctionLoader::loadImageFunctions() {
    vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
        vkGetDeviceProcAddr(device, "vkCreateImage"));
    vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        vkGetDeviceProcAddr(device, "vkDestroyImage"));
    vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
        vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements"));
    vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(
        vkGetDeviceProcAddr(device, "vkBindImageMemory"));
    vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
        vkGetDeviceProcAddr(device, "vkCreateImageView"));
    vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(
        vkGetDeviceProcAddr(device, "vkDestroyImageView"));
}

void VulkanFunctionLoader::loadSwapchainFunctions() {
    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
    
    // Load VK_EXT_swapchain_maintenance1 extension functions (optional)
    vkReleaseSwapchainImagesEXT = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(
        vkGetDeviceProcAddr(device, "vkReleaseSwapchainImagesEXT"));
    
}

void VulkanFunctionLoader::loadPipelineFunctions() {
    vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
        vkGetDeviceProcAddr(device, "vkCreateRenderPass"));
    vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(
        vkGetDeviceProcAddr(device, "vkDestroyRenderPass"));
    vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(
        vkGetDeviceProcAddr(device, "vkCreateFramebuffer"));
    vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(
        vkGetDeviceProcAddr(device, "vkDestroyFramebuffer"));
    vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
        vkGetDeviceProcAddr(device, "vkCreateShaderModule"));
    vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
        vkGetDeviceProcAddr(device, "vkDestroyShaderModule"));
    vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(
        vkGetDeviceProcAddr(device, "vkCreatePipelineLayout"));
    vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(
        vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout"));
    vkCreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(
        vkGetDeviceProcAddr(device, "vkCreatePipelineCache"));
    vkDestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(
        vkGetDeviceProcAddr(device, "vkDestroyPipelineCache"));
    vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
        vkGetDeviceProcAddr(device, "vkCreateGraphicsPipelines"));
    vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(
        vkGetDeviceProcAddr(device, "vkCreateComputePipelines"));
    vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(
        vkGetDeviceProcAddr(device, "vkDestroyPipeline"));
}

void VulkanFunctionLoader::loadDescriptorFunctions() {
    vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
        vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
        vkGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout"));
    vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(
        vkGetDeviceProcAddr(device, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
        vkGetDeviceProcAddr(device, "vkDestroyDescriptorPool"));
    vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(
        vkGetDeviceProcAddr(device, "vkResetDescriptorPool"));
    vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
        vkGetDeviceProcAddr(device, "vkAllocateDescriptorSets"));
    vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
        vkGetDeviceProcAddr(device, "vkUpdateDescriptorSets"));
}

void VulkanFunctionLoader::loadSynchronizationFunctions() {
    vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
        vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
    vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
        vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
    vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(
        vkGetDeviceProcAddr(device, "vkCreateFence"));
    vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(
        vkGetDeviceProcAddr(device, "vkDestroyFence"));
    vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(
        vkGetDeviceProcAddr(device, "vkWaitForFences"));
    vkResetFences = reinterpret_cast<PFN_vkResetFences>(
        vkGetDeviceProcAddr(device, "vkResetFences"));
    vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(
        vkGetDeviceProcAddr(device, "vkGetFenceStatus"));
}

void VulkanFunctionLoader::loadCommandFunctions() {
    vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(
        vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
    vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(
        vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
    vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
        vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(
        vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"));
    vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(
        vkGetDeviceProcAddr(device, "vkResetCommandBuffer"));
    vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
        vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
        vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
}

void VulkanFunctionLoader::loadRenderingFunctions() {
    vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
        vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass"));
    vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
        vkGetDeviceProcAddr(device, "vkCmdEndRenderPass"));
    vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(
        vkGetDeviceProcAddr(device, "vkCmdBindPipeline"));
    vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(
        vkGetDeviceProcAddr(device, "vkCmdSetViewport"));
    vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(
        vkGetDeviceProcAddr(device, "vkCmdSetScissor"));
    vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(
        vkGetDeviceProcAddr(device, "vkCmdDraw"));
    vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(
        vkGetDeviceProcAddr(device, "vkCmdDrawIndexed"));
    vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(
        vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets"));
    vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(
        vkGetDeviceProcAddr(device, "vkCmdBindVertexBuffers"));
    vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(
        vkGetDeviceProcAddr(device, "vkCmdBindIndexBuffer"));
    vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(
        vkGetDeviceProcAddr(device, "vkCmdDispatch"));
    vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
        vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier"));
    vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(
        vkGetDeviceProcAddr(device, "vkCmdPushConstants"));
    vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(
        vkGetDeviceProcAddr(device, "vkCmdCopyBuffer"));
    vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
        vkGetDeviceProcAddr(device, "vkCmdCopyBufferToImage"));
}

void VulkanFunctionLoader::loadQueueFunctions() {
    vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
        vkGetDeviceProcAddr(device, "vkQueueSubmit"));
    vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(
        vkGetDeviceProcAddr(device, "vkQueueWaitIdle"));
}