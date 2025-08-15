#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include "vulkan_context.h"

// Forward declaration
class VulkanFunctionLoader;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapchain {
public:
    VulkanSwapchain();
    ~VulkanSwapchain();

    bool initialize(VulkanContext* context, SDL_Window* window, VulkanFunctionLoader* loader = nullptr);
    void cleanup();
    bool recreate(VkRenderPass renderPass);

    VkSwapchainKHR getSwapchain() const { return swapChain; }
    const std::vector<VkImage>& getImages() const { return swapChainImages; }
    VkFormat getImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getExtent() const { return swapChainExtent; }
    const std::vector<VkImageView>& getImageViews() const { return swapChainImageViews; }
    
    VkImage getDepthImage() const { return depthImage; }
    VkImageView getDepthImageView() const { return depthImageView; }
    VkImage getMSAAColorImage() const { return msaaColorImage; }
    VkImageView getMSAAColorImageView() const { return msaaColorImageView; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapChainFramebuffers; }
    
    bool createFramebuffers(VkRenderPass renderPass);

private:
    VulkanContext* context = nullptr;
    SDL_Window* window = nullptr;
    VulkanFunctionLoader* loader = nullptr;
    
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    
    VkImage msaaColorImage = VK_NULL_HANDLE;
    VkDeviceMemory msaaColorImageMemory = VK_NULL_HANDLE;
    VkImageView msaaColorImageView = VK_NULL_HANDLE;
    
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Note: Function pointers removed - now using centralized VulkanFunctionLoader

    bool createSwapChain();
    bool createImageViews();
    bool createDepthResources();
    bool createMSAAColorResources();
    void cleanupSwapChain();
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits numSamples,
                     VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};