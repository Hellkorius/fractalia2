#include "vulkan_swapchain.h"
#include <iostream>
#include <algorithm>
#include <array>

VulkanSwapchain::VulkanSwapchain() {
}

VulkanSwapchain::~VulkanSwapchain() {
    cleanup();
}

bool VulkanSwapchain::initialize(VulkanContext* context, SDL_Window* window) {
    this->context = context;
    this->window = window;
    
    loadFunctions();
    
    std::cout << "Creating swap chain..." << std::endl;
    if (!createSwapChain()) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }
    std::cout << "Swap chain created successfully" << std::endl;
    
    std::cout << "Creating image views..." << std::endl;
    if (!createImageViews()) {
        std::cerr << "Failed to create image views" << std::endl;
        return false;
    }
    std::cout << "Image views created successfully" << std::endl;
    
    std::cout << "Creating depth resources..." << std::endl;
    if (!createDepthResources()) {
        std::cerr << "Failed to create depth resources" << std::endl;
        return false;
    }
    std::cout << "Depth resources created successfully" << std::endl;
    
    std::cout << "Creating MSAA color resources..." << std::endl;
    if (!createMSAAColorResources()) {
        std::cerr << "Failed to create MSAA color resources" << std::endl;
        return false;
    }
    std::cout << "MSAA color resources created successfully" << std::endl;
    
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

    context->vkDeviceWaitIdle(context->getDevice());

    cleanupSwapChain();

    if (!createSwapChain()) {
        std::cerr << "Failed to recreate swap chain!" << std::endl;
        return false;
    }
    
    if (!createImageViews()) {
        std::cerr << "Failed to recreate image views!" << std::endl;
        return false;
    }
    
    if (!createDepthResources()) {
        std::cerr << "Failed to recreate depth resources!" << std::endl;
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

bool VulkanSwapchain::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(context->getPhysicalDevice());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = context->findQueueFamilies(context->getPhysicalDevice());
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
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(context->getDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }

    vkGetSwapchainImagesKHR(context->getDevice(), swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context->getDevice(), swapChain, &imageCount, swapChainImages.data());

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

        if (vkCreateImageView(context->getDevice(), &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image views" << std::endl;
            return false;
        }
    }

    return true;
}

bool VulkanSwapchain::createDepthResources() {
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    
    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_SAMPLE_COUNT_4_BIT, depthImage, depthImageMemory);
    
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    
    return true;
}

bool VulkanSwapchain::createMSAAColorResources() {
    VkFormat colorFormat = swapChainImageFormat;
    
    createImage(swapChainExtent.width, swapChainExtent.height, colorFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_4_BIT, msaaColorImage, msaaColorImageMemory);
    
    msaaColorImageView = createImageView(msaaColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    
    return true;
}

void VulkanSwapchain::cleanupSwapChain() {
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(context->getDevice(), framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();
    
    if (msaaColorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context->getDevice(), msaaColorImageView, nullptr);
        msaaColorImageView = VK_NULL_HANDLE;
    }
    if (msaaColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(context->getDevice(), msaaColorImage, nullptr);
        msaaColorImage = VK_NULL_HANDLE;
    }
    if (msaaColorImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), msaaColorImageMemory, nullptr);
        msaaColorImageMemory = VK_NULL_HANDLE;
    }
    
    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context->getDevice(), depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }
    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(context->getDevice(), depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }
    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }
    
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(context->getDevice(), imageView, nullptr);
    }
    swapChainImageViews.clear();
    
    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context->getDevice(), swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}

SwapChainSupportDetails VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, context->getSurface(), &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, context->getSurface(), &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, context->getSurface(), &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, context->getSurface(), &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, context->getSurface(), &presentModeCount, details.presentModes.data());
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
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
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

void VulkanSwapchain::loadFunctions() {
    vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateSwapchainKHR");
    vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroySwapchainKHR");
    vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetSwapchainImagesKHR");
    vkCreateImageView = (PFN_vkCreateImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImageView");
    vkDestroyImageView = (PFN_vkDestroyImageView)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyImageView");
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)context->vkGetInstanceProcAddr(context->getInstance(), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)context->vkGetInstanceProcAddr(context->getInstance(), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)context->vkGetInstanceProcAddr(context->getInstance(), "vkGetPhysicalDeviceSurfacePresentModesKHR");
    vkCreateImage = (PFN_vkCreateImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateImage");
    vkDestroyImage = (PFN_vkDestroyImage)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyImage");
    vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)context->vkGetDeviceProcAddr(context->getDevice(), "vkGetImageMemoryRequirements");
    vkAllocateMemory = (PFN_vkAllocateMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkAllocateMemory");
    vkFreeMemory = (PFN_vkFreeMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkFreeMemory");
    vkBindImageMemory = (PFN_vkBindImageMemory)context->vkGetDeviceProcAddr(context->getDevice(), "vkBindImageMemory");
    vkCreateFramebuffer = (PFN_vkCreateFramebuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkCreateFramebuffer");
    vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkDestroyFramebuffer");
}

uint32_t VulkanSwapchain::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    context->vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanSwapchain::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits numSamples,
                                 VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(context->getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context->getDevice(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(context->getDevice(), image, imageMemory, 0);
}

VkImageView VulkanSwapchain::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(context->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }

    return imageView;
}

bool VulkanSwapchain::createFramebuffers(VkRenderPass renderPass) {
    swapChainFramebuffers.resize(swapChainImageViews.size());
    
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            msaaColorImageView,      
            swapChainImageViews[i],  
            depthImageView           
        };
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(context->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer" << std::endl;
            return false;
        }
    }

    return true;
}