#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include "vulkan_constants.h"
#include "vulkan_raii.h"

class VulkanSync {
public:
    VulkanSync();
    ~VulkanSync();

    bool initialize(const VulkanContext& context);
    void cleanup();
    
    // Explicit cleanup before context destruction
    void cleanupBeforeContextDestruction();

    VkCommandPool getCommandPool() const { return commandPool.get(); }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return commandBuffers; }
    // Get individual handles by index
    VkSemaphore getImageAvailableSemaphore(size_t index) const;
    VkSemaphore getRenderFinishedSemaphore(size_t index) const;
    VkSemaphore getComputeFinishedSemaphore(size_t index) const;
    VkFence getInFlightFence(size_t index) const;
    VkFence getComputeFence(size_t index) const;
    
    // Get full vectors (for compatibility)
    std::vector<VkSemaphore> getImageAvailableSemaphores() const;
    std::vector<VkSemaphore> getRenderFinishedSemaphores() const;
    std::vector<VkSemaphore> getComputeFinishedSemaphores() const;
    std::vector<VkFence> getInFlightFences() const;
    std::vector<VkFence> getComputeFences() const;
    const std::vector<VkCommandBuffer>& getComputeCommandBuffers() const { return computeCommandBuffers; }
    
    // Optimized command buffer management
    void resetCommandBuffersForFrame(uint32_t frameIndex);
    void resetAllCommandBuffers();
    
    // CRITICAL FIX: Command pool recreation for resize corruption fix
    bool recreateCommandPool();

private:
    const VulkanContext* context = nullptr;
    
    vulkan_raii::CommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;        // Graphics command buffers
    std::vector<VkCommandBuffer> computeCommandBuffers; // Compute command buffers
    std::vector<vulkan_raii::Semaphore> imageAvailableSemaphores;
    std::vector<vulkan_raii::Semaphore> renderFinishedSemaphores;
    std::vector<vulkan_raii::Semaphore> computeFinishedSemaphores; // Compute-to-graphics synchronization
    std::vector<vulkan_raii::Fence> inFlightFences;                // Graphics fences
    std::vector<vulkan_raii::Fence> computeFences;                 // Compute fences

    bool createCommandPool();
    bool createCommandBuffers(); 
    bool createSyncObjects();
};