#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

// Centralized Vulkan function pointer loading for all modules
// Provides single source of truth for Vulkan function management
class VulkanFunctionLoader {
public:
    VulkanFunctionLoader();
    ~VulkanFunctionLoader();
    
    // Initialize function loading with SDL window
    bool initialize(SDL_Window* window);
    void cleanup();
    
    // Set instance and device handles (called by VulkanContext)
    void setInstance(VkInstance instance) { this->instance = instance; }
    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice) { 
        this->device = device; 
        this->physicalDevice = physicalDevice;
    }
    
    // Public loading methods (called by VulkanContext after instance/device creation)
    void loadPostInstanceFunctions();
    bool loadPostDeviceFunctions();
    
    // Core instance/device handles
    VkInstance getInstance() const { return instance; }
    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    
    // === INSTANCE FUNCTIONS ===
    // Core instance functions
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
    
    // Physical device functions
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties = nullptr;
    
    // Surface functions
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
    
    // === DEVICE FUNCTIONS ===
    // Device management
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
    
    // Memory management
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    
    // Buffer management
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    
    // Image management
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    
    // Swapchain functions
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
    
    // VK_EXT_swapchain_maintenance1 extension functions (optional)
    PFN_vkReleaseSwapchainImagesEXT vkReleaseSwapchainImagesEXT = nullptr;
    
    // Render pass and pipeline functions
    PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkCreatePipelineCache vkCreatePipelineCache = nullptr;
    PFN_vkDestroyPipelineCache vkDestroyPipelineCache = nullptr;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
    PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    
    // Descriptor set functions
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    
    // Synchronization functions
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;
    PFN_vkWaitForFences vkWaitForFences = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
    
    // Command buffer functions
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
    PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    
    // Command recording functions
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
    PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
    PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
    PFN_vkCmdDraw vkCmdDraw = nullptr;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
    PFN_vkCmdDispatch vkCmdDispatch = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdPushConstants vkCmdPushConstants = nullptr;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
    // Note: vkCmdTransitionImageLayout is not a real Vulkan function - removed
    
    // Queue functions
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
    
private:
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    
    // Private loading functions
    bool loadInstanceFunctions();
    bool loadDeviceFunctions();
    void loadCoreInstanceFunctions();
    void loadPhysicalDeviceFunctions();
    void loadSurfaceFunctions();
    void loadDeviceManagementFunctions();
    void loadMemoryFunctions();
    void loadBufferFunctions();
    void loadImageFunctions();
    void loadSwapchainFunctions();
    void loadPipelineFunctions();
    void loadDescriptorFunctions();
    void loadSynchronizationFunctions();
    void loadCommandFunctions();
    void loadRenderingFunctions();
    void loadQueueFunctions();
};