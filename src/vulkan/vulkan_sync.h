#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include "vulkan_constants.h"

class VulkanSync {
public:
    VulkanSync();
    ~VulkanSync();

    bool initialize(VulkanContext* context, VulkanFunctionLoader* loader);
    void cleanup();

    VkCommandPool getCommandPool() const { return commandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return commandBuffers; }
    const std::vector<VkSemaphore>& getImageAvailableSemaphores() const { return imageAvailableSemaphores; }
    const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const { return renderFinishedSemaphores; }
    const std::vector<VkFence>& getInFlightFences() const { return inFlightFences; }
    const std::vector<VkFence>& getComputeFences() const { return computeFences; }
    const std::vector<VkCommandBuffer>& getComputeCommandBuffers() const { return computeCommandBuffers; }

private:
    VulkanContext* context = nullptr;
    VulkanFunctionLoader* loader = nullptr;
    
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;        // Graphics command buffers
    std::vector<VkCommandBuffer> computeCommandBuffers; // Compute command buffers
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;                // Graphics fences
    std::vector<VkFence> computeFences;                 // Compute fences

    bool createCommandPool();
    bool createCommandBuffers(); 
    bool createSyncObjects();
};