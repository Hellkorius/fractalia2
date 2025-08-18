#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

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
        VkFence fence = VK_NULL_HANDLE;
        bool completed = false;
    };
    
    AsyncTransfer copyBufferToBufferAsync(VkBuffer src, VkBuffer dst, VkDeviceSize size,
                                         VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    bool isTransferComplete(const AsyncTransfer& transfer);
    void waitForTransfer(const AsyncTransfer& transfer);
    void freeAsyncTransfer(AsyncTransfer& transfer);
    
private:
    const VulkanContext* context = nullptr;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandPool transferCommandPool = VK_NULL_HANDLE;
    
    bool createTransferCommandPool();
    void cleanupTransferCommandPool();
};