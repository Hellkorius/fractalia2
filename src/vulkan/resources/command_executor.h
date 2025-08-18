#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../core/vulkan_raii.h"

class VulkanContext;

class CommandExecutor {
public:
    CommandExecutor();
    ~CommandExecutor();
    
    bool initialize(const VulkanContext& context, VkCommandPool commandPool);
    void cleanup();
    
    // Synchronous transfer (existing)
    void copyBufferToBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, 
                           VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Async transfer with transfer queue (new)
    struct AsyncTransfer {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        vulkan_raii::Fence fence;
        bool completed = false;
    };
    
    AsyncTransfer copyBufferToBufferAsync(VkBuffer src, VkBuffer dst, VkDeviceSize size,
                                         VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    bool isTransferComplete(const AsyncTransfer& transfer);
    void waitForTransfer(const AsyncTransfer& transfer);
    void freeAsyncTransfer(AsyncTransfer& transfer);
    
    // Cleanup method for proper destruction order
    void cleanupBeforeContextDestruction();
    
private:
    const VulkanContext* context = nullptr;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    vulkan_raii::CommandPool transferCommandPool;
    
    bool createTransferCommandPool();
    void cleanupTransferCommandPool();
};