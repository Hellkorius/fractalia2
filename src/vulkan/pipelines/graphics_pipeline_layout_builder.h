#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "../core/vulkan_context.h"


class GraphicsPipelineLayoutBuilder {
public:
    explicit GraphicsPipelineLayoutBuilder(VulkanContext* ctx);
    ~GraphicsPipelineLayoutBuilder() = default;

    VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                         const std::vector<VkPushConstantRange>& pushConstants);

private:
    VulkanContext* context;
    
    void logLayoutCreation(const std::vector<VkDescriptorSetLayout>& setLayouts,
                          const std::vector<VkPushConstantRange>& pushConstants,
                          VkPipelineLayout layout) const;
};