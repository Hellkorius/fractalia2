#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include "vulkan_constants.h"
#include "vulkan_raii.h"

/**
 * @brief Pure Vulkan synchronization object management
 * 
 * Manages semaphores and fences for GPU-CPU and GPU-GPU synchronization.
 * Command buffer management is handled by QueueManager for clean separation of concerns.
 */
class VulkanSync {
public:
    VulkanSync();
    ~VulkanSync();

    bool initialize(const VulkanContext& context);
    void cleanup();
    
    // Explicit cleanup before context destruction
    void cleanupBeforeContextDestruction();
    
    // Synchronization object access
    VkSemaphore getImageAvailableSemaphore(size_t index) const;
    VkSemaphore getRenderFinishedSemaphore(size_t index) const;
    VkSemaphore getComputeFinishedSemaphore(size_t index) const;
    VkFence getInFlightFence(size_t index) const;
    VkFence getComputeFence(size_t index) const;
    
    // Get full vectors (for compatibility with existing code)
    std::vector<VkSemaphore> getImageAvailableSemaphores() const;
    std::vector<VkSemaphore> getRenderFinishedSemaphores() const;
    std::vector<VkSemaphore> getComputeFinishedSemaphores() const;
    std::vector<VkFence> getInFlightFences() const;
    std::vector<VkFence> getComputeFences() const;
    
private:
    const VulkanContext* context = nullptr;
    
    // Synchronization objects (core responsibility of VulkanSync)
    std::vector<vulkan_raii::Semaphore> imageAvailableSemaphores;
    std::vector<vulkan_raii::Semaphore> renderFinishedSemaphores;
    std::vector<vulkan_raii::Semaphore> computeFinishedSemaphores; // Compute-to-graphics synchronization
    std::vector<vulkan_raii::Fence> inFlightFences;                // Graphics fences
    std::vector<vulkan_raii::Fence> computeFences;                 // Compute fences

    // Internal methods
    bool createSyncObjects();
};