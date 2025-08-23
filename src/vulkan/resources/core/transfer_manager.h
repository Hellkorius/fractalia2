#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "resource_handle.h"
#include "command_executor.h"

class TransferOrchestrator;

// Pure transfer operations - single responsibility  
class TransferManager {
public:
    TransferManager() = default;
    ~TransferManager() = default;
    
    bool initialize(TransferOrchestrator* transferOrchestrator);
    void cleanup();
    
    // Transfer operations
    bool copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    bool copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, 
                           VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    CommandExecutor::AsyncTransfer copyToBufferAsync(const ResourceHandle& dst, const void* data, 
                                                     VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Access to underlying orchestrator for advanced operations
    TransferOrchestrator* getTransferOrchestrator() const { return transferOrchestrator; }

private:
    TransferOrchestrator* transferOrchestrator = nullptr;
    bool initialized = false;
};