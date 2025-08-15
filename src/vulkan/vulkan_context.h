#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <optional>
#include <vector>
#include <memory>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool initialize(SDL_Window* window);
    void cleanup();

    VkInstance getInstance() const { return instance; }
    VkSurfaceKHR getSurface() const { return surface; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    
    class VulkanFunctionLoader& getLoader() const { return *loader; }
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    void getDeviceQueues(); // Call this after device functions are loaded


private:
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices; // Store indices for queue retrieval after device creation

    std::unique_ptr<class VulkanFunctionLoader> loader;

    bool createInstance();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    
    std::vector<const char*> getRequiredExtensions();
};