#include "graphics_pipeline_layout_builder.h"
#include "../core/vulkan_function_loader.h"
#include <iostream>

GraphicsPipelineLayoutBuilder::GraphicsPipelineLayoutBuilder(VulkanContext* ctx) : context(ctx) {
}

VkPipelineLayout GraphicsPipelineLayoutBuilder::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                                                    const std::vector<VkPushConstantRange>& pushConstants) {
    std::cout << "GraphicsPipelineLayoutBuilder: Creating graphics pipeline layout" << std::endl;
    std::cout << "  Descriptor set layouts count: " << setLayouts.size() << std::endl;
    for (size_t i = 0; i < setLayouts.size(); ++i) {
        std::cout << "    Layout[" << i << "]: " << (void*)setLayouts[i] << (setLayouts[i] != VK_NULL_HANDLE ? " (valid)" : " (INVALID!)") << std::endl;
    }
    std::cout << "  Push constant ranges count: " << pushConstants.size() << std::endl;
    
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
    layoutInfo.pPushConstantRanges = pushConstants.data();
    
    VkPipelineLayout layout;
    const auto& vk = context->getLoader();
    VkResult result = vk.vkCreatePipelineLayout(context->getDevice(), &layoutInfo, nullptr, &layout);
    
    if (result != VK_SUCCESS) {
        std::cerr << "GraphicsPipelineLayoutBuilder: CRITICAL ERROR - Failed to create graphics pipeline layout!" << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        std::cerr << "  Device valid: " << (context->getDevice() != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        return VK_NULL_HANDLE;
    }
    
    logLayoutCreation(setLayouts, pushConstants, layout);
    return layout;
}

void GraphicsPipelineLayoutBuilder::logLayoutCreation(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                                     const std::vector<VkPushConstantRange>& pushConstants,
                                                     VkPipelineLayout layout) const {
    std::cout << "GraphicsPipelineLayoutBuilder: Graphics pipeline layout created successfully: " << (void*)layout << std::endl;
}