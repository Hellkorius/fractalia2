#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "../core/vulkan_context.h"

#include "../core/vulkan_raii.h"

class GraphicsRenderPassManager {
public:
    explicit GraphicsRenderPassManager(VulkanContext* ctx);
    ~GraphicsRenderPassManager() = default;

    VkRenderPass createRenderPass(VkFormat colorFormat, 
                                 VkFormat depthFormat = VK_FORMAT_UNDEFINED,
                                 VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                                 bool enableMSAA = false);
    
    void clearCache();
    size_t getCacheSize() const { return renderPassCache_.size(); }

private:
    VulkanContext* context;
    std::unordered_map<size_t, vulkan_raii::RenderPass> renderPassCache_;
    
    size_t createRenderPassHash(VkFormat colorFormat, VkFormat depthFormat, 
                               VkSampleCountFlagBits samples, bool enableMSAA) const;
};