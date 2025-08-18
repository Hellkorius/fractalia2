#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_raii.h"


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapchain {
public:
    VulkanSwapchain();
    ~VulkanSwapchain();

    bool initialize(const VulkanContext& context, SDL_Window* window);
    void cleanup();
    bool recreate(VkRenderPass renderPass);
    
    // Explicit cleanup before context destruction
    void cleanupBeforeContextDestruction();

    VkSwapchainKHR getSwapchain() const { return swapChain; }
    const std::vector<VkImage>& getImages() const { return swapChainImages; }
    VkFormat getImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getExtent() const { return swapChainExtent; }
    std::vector<VkImageView> getImageViews() const;
    
    VkImage getMSAAColorImage() const { return msaaColorImage.get(); }
    VkImageView getMSAAColorImageView() const { return msaaColorImageView.get(); }
    std::vector<VkFramebuffer> getFramebuffers() const;
    
    bool createFramebuffers(VkRenderPass renderPass);

private:
    const VulkanContext* context = nullptr;
    SDL_Window* window = nullptr;
    
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<vulkan_raii::ImageView> swapChainImageViews;
    
    
    vulkan_raii::Image msaaColorImage;
    vulkan_raii::DeviceMemory msaaColorImageMemory;
    vulkan_raii::ImageView msaaColorImageView;
    
    std::vector<vulkan_raii::Framebuffer> swapChainFramebuffers;


    bool createSwapChain(VkSwapchainKHR oldSwapchainKHR = VK_NULL_HANDLE);
    bool createImageViews();
    bool createMSAAColorResources();
    void cleanupSwapChain();
    void cleanupSwapChainExceptSwapchain();
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
};