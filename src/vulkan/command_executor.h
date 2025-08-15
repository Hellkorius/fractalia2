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
    
    void copyBufferToBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, 
                           VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
private:
    const VulkanContext* context = nullptr;
    VkCommandPool commandPool = VK_NULL_HANDLE;
};