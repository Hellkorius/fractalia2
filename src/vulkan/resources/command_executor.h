#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../core/vulkan_raii.h"
#include "../core/queue_manager.h"

class VulkanContext;

/**
 * @brief Modern command executor using QueueManager for optimal queue selection
 * 
 * Provides synchronous and asynchronous buffer operations with automatic
 * queue selection (dedicated transfer queue when available, graphics fallback).
 */
class CommandExecutor {
public:
    CommandExecutor();
    ~CommandExecutor();
    
    bool initialize(const VulkanContext& context, QueueManager* queueManager);
    void cleanup();
    
    // Synchronous transfer (uses graphics queue for immediate completion)
    void copyBufferToBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, 
                           VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Async transfer with optimal queue selection
    using AsyncTransfer = QueueManager::TransferCommand;
    
    AsyncTransfer copyBufferToBufferAsync(VkBuffer src, VkBuffer dst, VkDeviceSize size,
                                         VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    bool isTransferComplete(const AsyncTransfer& transfer);
    void waitForTransfer(const AsyncTransfer& transfer);
    void freeAsyncTransfer(AsyncTransfer& transfer);
    
    // Queue capability queries
    bool usesDedicatedTransferQueue() const;
    VkQueue getTransferQueue() const;
    uint32_t getTransferQueueFamily() const;
    
    // Cleanup method for proper destruction order
    void cleanupBeforeContextDestruction();
    
private:
    const VulkanContext* context = nullptr;
    QueueManager* queueManager = nullptr;
};