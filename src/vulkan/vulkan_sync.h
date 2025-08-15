#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_constants.h"

class VulkanSync {
public:
    VulkanSync();
    ~VulkanSync();

    bool initialize(VulkanContext* context);
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
    
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;        // Graphics command buffers
    std::vector<VkCommandBuffer> computeCommandBuffers; // Compute command buffers
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;                // Graphics fences
    std::vector<VkFence> computeFences;                 // Compute fences

    // Function pointers for sync operations
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;

    bool createCommandPool();
    bool createCommandBuffers(); 
    bool createSyncObjects();
    
    void loadFunctions();
};