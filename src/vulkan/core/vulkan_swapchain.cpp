#include "vulkan_swapchain.h"
#include "vulkan_function_loader.h"
#include "vulkan_utils.h"
#include "vulkan_constants.h"
#include <iostream>
#include <algorithm>
#include <array>

VulkanSwapchain::VulkanSwapchain() {
}

VulkanSwapchain::~VulkanSwapchain() {
    cleanup();
}

bool VulkanSwapchain::initialize(const VulkanContext& context, SDL_Window* window) {
    this->context = &context;
    this->window = window;
    
    if (!createSwapChain(VK_NULL_HANDLE)) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }
    
    if (!createImageViews()) {
        std::cerr << "Failed to create image views" << std::endl;
        return false;
    }
    
    
    if (!createMSAAColorResources()) {
        std::cerr << "Failed to create MSAA color resources" << std::endl;
        return false;
    }
    
    return true;
}

void VulkanSwapchain::cleanup() {
    cleanupSwapChain();
}

bool VulkanSwapchain::recreate(VkRenderPass renderPass) {
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    
    while (width == 0 || height == 0) {
        SDL_GetWindowSizeInPixels(window, &width, &height);  
        SDL_WaitEvent(nullptr);
    }

    // Store old swapchain for proper recreation
    VkSwapchainKHR oldSwapchain = swapChain;
    
    // Clean up everything except the old swapchain itself
    cleanupSwapChainExceptSwapchain();

    if (!createSwapChain(oldSwapchain)) {
        std::cerr << "Failed to recreate swap chain!" << std::endl;
        // Cache loader and device references for performance
        const auto& vk = context->getLoader();
        const VkDevice device = context->getDevice();
        
        // If creation failed, we still need to clean up the old swapchain
        if (oldSwapchain != VK_NULL_HANDLE) {
            vk.vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
        }
        return false;
    }
    
    // Cache loader and device references for performance
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    // Now destroy the old swapchain after successful recreation
    if (oldSwapchain != VK_NULL_HANDLE) {
        vk.vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }
    
    if (!createImageViews()) {
        std::cerr << "Failed to recreate image views!" << std::endl;
        return false;
    }
    
    
    if (!createMSAAColorResources()) {
        std::cerr << "Failed to recreate MSAA color resources!" << std::endl;
        return false;
    }
    
    if (!createFramebuffers(renderPass)) {
        std::cerr << "Failed to recreate framebuffers!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanSwapchain::createSwapChain(VkSwapchainKHR oldSwapchainKHR) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(context->getPhysicalDevice());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // Optimize image count for VulkanRenderer::MAX_FRAMES_IN_FLIGHT
    // Request minImageCount + frames_in_flight to ensure both engine and compositor have spare buffers
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + MAX_FRAMES_IN_FLIGHT;
    
    // Clamp to supported range
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
        std::cout << "WARNING: Swapchain image count clamped to " << imageCount 
                  << " (requested " << (swapChainSupport.capabilities.minImageCount + MAX_FRAMES_IN_FLIGHT) 
                  << ")" << std::endl;
    }
    
    std::cout << "Creating swapchain with " << imageCount << " images (min=" 
              << swapChainSupport.capabilities.minImageCount 
              << ", max=" << swapChainSupport.capabilities.maxImageCount << ")" << std::endl;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Use cached queue family indices instead of re-querying during swapchain recreation
    const QueueFamilyIndices& indices = context->getQueueFamilyIndices();
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchainKHR;

    // Cache loader and device references for performance
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();

    if (vk.vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }

    vk.vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vk.vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    return true;
}

bool VulkanSwapchain::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (context->getLoader().vkCreateImageView(context->getDevice(), &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image views" << std::endl;
            return false;
        }
    }

    return true;
}


bool VulkanSwapchain::createMSAAColorResources() {
    VkFormat colorFormat = swapChainImageFormat;
    
    VulkanUtils::createImage(context->getDevice(), context->getPhysicalDevice(), context->getLoader(),
                            swapChainExtent.width, swapChainExtent.height, colorFormat, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, msaaColorImage, msaaColorImageMemory, VK_SAMPLE_COUNT_2_BIT);
    
    msaaColorImageView = VulkanUtils::createImageView(context->getDevice(), context->getLoader(), msaaColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    
    return true;
}

void VulkanSwapchain::cleanupSwapChain() {
    // Cache loader and device references for performance
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    for (auto framebuffer : swapChainFramebuffers) {
        vk.vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();
    
    if (msaaColorImageView != VK_NULL_HANDLE) {
        vk.vkDestroyImageView(device, msaaColorImageView, nullptr);
        msaaColorImageView = VK_NULL_HANDLE;
    }
    if (msaaColorImage != VK_NULL_HANDLE) {
        vk.vkDestroyImage(device, msaaColorImage, nullptr);
        msaaColorImage = VK_NULL_HANDLE;
    }
    if (msaaColorImageMemory != VK_NULL_HANDLE) {
        vk.vkFreeMemory(device, msaaColorImageMemory, nullptr);
        msaaColorImageMemory = VK_NULL_HANDLE;
    }
    
    
    for (auto imageView : swapChainImageViews) {
        vk.vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();
    
    if (swapChain != VK_NULL_HANDLE) {
        vk.vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::cleanupSwapChainExceptSwapchain() {
    std::cout << "VulkanSwapchain: Starting cleanup of swapchain resources (except swapchain handle)" << std::endl;
    
    // Cleanup framebuffers
    std::cout << "VulkanSwapchain: Destroying " << swapChainFramebuffers.size() << " framebuffers" << std::endl;
    for (auto framebuffer : swapChainFramebuffers) {
        context->getLoader().vkDestroyFramebuffer(context->getDevice(), framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();
    
    // Cleanup MSAA resources - CRITICAL for memory leak detection
    if (msaaColorImageView != VK_NULL_HANDLE) {
        std::cout << "VulkanSwapchain: Destroying MSAA color image view" << std::endl;
        context->getLoader().vkDestroyImageView(context->getDevice(), msaaColorImageView, nullptr);
        msaaColorImageView = VK_NULL_HANDLE;
    }
    if (msaaColorImage != VK_NULL_HANDLE) {
        std::cout << "VulkanSwapchain: Destroying MSAA color image" << std::endl;
        context->getLoader().vkDestroyImage(context->getDevice(), msaaColorImage, nullptr);
        msaaColorImage = VK_NULL_HANDLE;
    }
    if (msaaColorImageMemory != VK_NULL_HANDLE) {
        std::cout << "VulkanSwapchain: CRITICAL - Freeing MSAA color image memory" << std::endl;
        context->getLoader().vkFreeMemory(context->getDevice(), msaaColorImageMemory, nullptr);
        msaaColorImageMemory = VK_NULL_HANDLE;
        std::cout << "VulkanSwapchain: MSAA memory successfully freed" << std::endl;
    }
    
    // Cleanup swapchain image views
    std::cout << "VulkanSwapchain: Destroying " << swapChainImageViews.size() << " image views" << std::endl;
    for (auto imageView : swapChainImageViews) {
        context->getLoader().vkDestroyImageView(context->getDevice(), imageView, nullptr);
    }
    swapChainImageViews.clear();
    
    std::cout << "VulkanSwapchain: Cleanup completed successfully" << std::endl;
    // Note: We deliberately do NOT destroy the swapchain here - that's handled separately
}

SwapChainSupportDetails VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    context->getLoader().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, context->getSurface(), &details.capabilities);

    uint32_t formatCount;
    context->getLoader().vkGetPhysicalDeviceSurfaceFormatsKHR(device, context->getSurface(), &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        context->getLoader().vkGetPhysicalDeviceSurfaceFormatsKHR(device, context->getSurface(), &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    context->getLoader().vkGetPhysicalDeviceSurfacePresentModesKHR(device, context->getSurface(), &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        context->getLoader().vkGetPhysicalDeviceSurfacePresentModesKHR(device, context->getSurface(), &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Priority order for low-latency, tearing-free presentation:
    // 1. MAILBOX - Low latency with triple buffering, no tearing
    // 2. IMMEDIATE - Lowest latency but allows tearing
    // 3. FIFO - Guaranteed no tearing, standard vsync
    
    // First choice: MAILBOX for optimal balance of low latency + no tearing
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            std::cout << "Using VK_PRESENT_MODE_MAILBOX_KHR for low-latency presentation" << std::endl;
            return availablePresentMode;
        }
    }
    
    // Second choice: IMMEDIATE for absolute minimum latency (allows tearing)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            std::cout << "Using VK_PRESENT_MODE_IMMEDIATE_KHR for minimum latency (may tear)" << std::endl;
            return availablePresentMode;
        }
    }
    
    // Fallback: FIFO is guaranteed to be available
    std::cout << "Using VK_PRESENT_MODE_FIFO_KHR fallback" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}


bool VulkanSwapchain::createFramebuffers(VkRenderPass renderPass) {
    swapChainFramebuffers.resize(swapChainImageViews.size());
    
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            msaaColorImageView,      
            swapChainImageViews[i]
        };
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (context->getLoader().vkCreateFramebuffer(context->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer" << std::endl;
            return false;
        }
    }

    return true;
}