#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <optional>
#include <vector>
#include <memory>
#include "vulkan_raii.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool initialize(SDL_Window* window);
    void cleanup();
    
    // Explicit cleanup before context destruction
    void cleanupBeforeContextDestruction();

    VkInstance getInstance() const { return instance.get(); }
    VkSurfaceKHR getSurface() const { return surface.get(); }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device.get(); }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    VkQueue getComputeQueue() const { return computeQueue; }
    uint32_t getGraphicsQueueFamily() const { return queueFamilyIndices.graphicsFamily.value(); }
    uint32_t getComputeQueueFamily() const { return queueFamilyIndices.computeFamily.value(); }
    uint32_t getPresentQueueFamily() const { return queueFamilyIndices.presentFamily.value(); }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return queueFamilyIndices; }
    
    class VulkanFunctionLoader& getLoader() const { return *loader; }
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    void getDeviceQueues(); // Call this after device functions are loaded


private:
    SDL_Window* window = nullptr;
    vulkan_raii::Instance instance;
    vulkan_raii::SurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // Physical devices are not owned
    vulkan_raii::Device device;
    VkQueue graphicsQueue = VK_NULL_HANDLE; // Queues are not owned
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices; // Store indices for queue retrieval after device creation

    std::unique_ptr<class VulkanFunctionLoader> loader;
    vulkan_raii::DebugUtilsMessengerEXT debugMessenger;

    bool createInstance();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool setupDebugMessenger();
    void cleanupDebugMessenger();
    
    std::vector<const char*> getRequiredExtensions();
};