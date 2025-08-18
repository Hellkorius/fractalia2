#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include "../core/vulkan_context.h"
#include "../core/vulkan_manager_base.h"

class GraphicsPipelineLayoutBuilder : public VulkanManagerBase {
public:
    explicit GraphicsPipelineLayoutBuilder(VulkanContext* ctx);
    ~GraphicsPipelineLayoutBuilder() = default;

    VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                         const std::vector<VkPushConstantRange>& pushConstants);

private:
    void logLayoutCreation(const std::vector<VkDescriptorSetLayout>& setLayouts,
                          const std::vector<VkPushConstantRange>& pushConstants,
                          VkPipelineLayout layout) const;
};