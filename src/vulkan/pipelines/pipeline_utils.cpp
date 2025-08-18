#include "pipeline_utils.h"
#include "../core/vulkan_function_loader.h"
#include <iostream>

// Pipeline layout utilities implementation
VkPipelineLayout PipelineUtils::createSimplePipelineLayout(VkDevice device,
                                                          const VulkanFunctionLoader& loader,
                                                          const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                                          const std::vector<VkPushConstantRange>& pushConstantRanges) {
    const auto& vk = loader;
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    if (!descriptorSetLayouts.empty()) {
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    }
    
    if (!pushConstantRanges.empty()) {
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
    }
    
    VkPipelineLayout pipelineLayout;
    if (vk.vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return pipelineLayout;
}

// Render pass utilities implementation
VkRenderPass PipelineUtils::createBasicRenderPass(VkDevice device,
                                                 const VulkanFunctionLoader& loader,
                                                 VkFormat colorFormat,
                                                 VkFormat depthFormat,
                                                 VkSampleCountFlagBits samples,
                                                 bool enableMSAA) {
    const auto& vk = loader;
    
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    VkAttachmentReference depthAttachmentRef{};
    
    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = samples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = enableMSAA ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments.push_back(colorAttachment);
    
    colorAttachmentRefs.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    
    // MSAA resolve attachment
    if (enableMSAA) {
        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = colorFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(colorAttachmentResolve);
    }
    
    // Depth attachment
    bool hasDepth = (depthFormat != VK_FORMAT_UNDEFINED);
    if (hasDepth) {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = samples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachment);
        
        depthAttachmentRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    
    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
    
    if (enableMSAA) {
        static VkAttachmentReference resolveAttachmentRef = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        subpass.pResolveAttachments = &resolveAttachmentRef;
    }
    
    if (hasDepth) {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }
    
    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    if (hasDepth) {
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    // Render pass create info
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    VkRenderPass renderPass;
    if (vk.vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return renderPass;
}

VkRenderPass PipelineUtils::createMSAARenderPass(VkDevice device,
                                               const VulkanFunctionLoader& loader,
                                               VkFormat colorFormat,
                                               VkFormat depthFormat,
                                               VkSampleCountFlagBits samples) {
    return createBasicRenderPass(device, loader, colorFormat, depthFormat, samples, true);
}

// Shader stage utilities implementation
VkPipelineShaderStageCreateInfo PipelineUtils::createShaderStageInfo(VkShaderStageFlagBits stage,
                                                                    VkShaderModule module,
                                                                    const char* entryPoint,
                                                                    const VkSpecializationInfo* specializationInfo) {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = module;
    shaderStageInfo.pName = entryPoint;
    shaderStageInfo.pSpecializationInfo = specializationInfo;
    
    return shaderStageInfo;
}

// Vertex input state utilities implementation
VkPipelineVertexInputStateCreateInfo PipelineUtils::createEmptyVertexInputState() {
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    
    return vertexInputInfo;
}

VkPipelineVertexInputStateCreateInfo PipelineUtils::createVertexInputState(
    const std::vector<VkVertexInputBindingDescription>& bindings,
    const std::vector<VkVertexInputAttributeDescription>& attributes) {
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInputInfo.pVertexBindingDescriptions = bindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributes.data();
    
    return vertexInputInfo;
}

// Input assembly utilities implementation
VkPipelineInputAssemblyStateCreateInfo PipelineUtils::createInputAssemblyState(VkPrimitiveTopology topology,
                                                                              bool primitiveRestart) {
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = primitiveRestart ? VK_TRUE : VK_FALSE;
    
    return inputAssembly;
}

// Viewport state utilities implementation
VkPipelineViewportStateCreateInfo PipelineUtils::createViewportState(uint32_t viewportCount,
                                                                    uint32_t scissorCount) {
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = viewportCount;
    viewportState.scissorCount = scissorCount;
    
    return viewportState;
}

// Rasterization state utilities implementation
VkPipelineRasterizationStateCreateInfo PipelineUtils::createRasterizationState(VkPolygonMode polygonMode,
                                                                              VkCullModeFlags cullMode,
                                                                              VkFrontFace frontFace,
                                                                              float lineWidth) {
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = polygonMode;
    rasterizer.lineWidth = lineWidth;
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    return rasterizer;
}

// Multisample state utilities implementation
VkPipelineMultisampleStateCreateInfo PipelineUtils::createMultisampleState(VkSampleCountFlagBits samples,
                                                                          bool enableSampleShading,
                                                                          float minSampleShading) {
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = enableSampleShading ? VK_TRUE : VK_FALSE;
    multisampling.rasterizationSamples = samples;
    multisampling.minSampleShading = minSampleShading;
    
    return multisampling;
}

// Depth stencil state utilities implementation
VkPipelineDepthStencilStateCreateInfo PipelineUtils::createDepthStencilState(bool depthTest,
                                                                            bool depthWrite,
                                                                            VkCompareOp depthCompareOp) {
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = depthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    return depthStencil;
}

// Color blend state utilities implementation
VkPipelineColorBlendStateCreateInfo PipelineUtils::createColorBlendState(
    const std::vector<VkPipelineColorBlendAttachmentState>& attachments) {
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(attachments.size());
    colorBlending.pAttachments = attachments.data();
    
    return colorBlending;
}

VkPipelineColorBlendAttachmentState PipelineUtils::createColorBlendAttachment(bool enableBlend,
                                                                            VkBlendFactor srcColorBlendFactor,
                                                                            VkBlendFactor dstColorBlendFactor) {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = enableBlend ? VK_TRUE : VK_FALSE;
    
    if (enableBlend) {
        colorBlendAttachment.srcColorBlendFactor = srcColorBlendFactor;
        colorBlendAttachment.dstColorBlendFactor = dstColorBlendFactor;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    
    return colorBlendAttachment;
}

// Dynamic state utilities implementation
VkPipelineDynamicStateCreateInfo PipelineUtils::createDynamicState(const std::vector<VkDynamicState>& dynamicStates) {
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    return dynamicState;
}

// Compute pipeline utilities implementation
VkComputePipelineCreateInfo PipelineUtils::createComputePipelineInfo(VkPipelineLayout layout,
                                                                    VkPipelineShaderStageCreateInfo shaderStage,
                                                                    VkPipeline basePipeline) {
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = layout;
    pipelineInfo.stage = shaderStage;
    pipelineInfo.basePipelineHandle = basePipeline;
    pipelineInfo.basePipelineIndex = -1;
    
    return pipelineInfo;
}

// Pipeline barrier utilities implementation
VkMemoryBarrier PipelineUtils::createMemoryBarrier(VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    
    return barrier;
}

VkBufferMemoryBarrier PipelineUtils::createBufferBarrier(VkBuffer buffer,
                                                        VkAccessFlags srcAccess,
                                                        VkAccessFlags dstAccess,
                                                        uint32_t srcQueueFamily,
                                                        uint32_t dstQueueFamily,
                                                        VkDeviceSize offset,
                                                        VkDeviceSize size) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    
    return barrier;
}

VkImageMemoryBarrier PipelineUtils::createImageBarrier(VkImage image,
                                                      VkAccessFlags srcAccess,
                                                      VkAccessFlags dstAccess,
                                                      VkImageLayout oldLayout,
                                                      VkImageLayout newLayout,
                                                      VkImageAspectFlags aspectMask,
                                                      uint32_t srcQueueFamily,
                                                      uint32_t dstQueueFamily) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    return barrier;
}

// Error handling utilities implementation
bool PipelineUtils::checkPipelineCreation(VkResult result, const char* pipelineType) {
    if (result == VK_SUCCESS) {
        return true;
    }
    
    std::cerr << "PipelineUtils: Failed to create " << pipelineType << " pipeline: ";
    
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_INVALID_SHADER_NV:
            std::cerr << "VK_ERROR_INVALID_SHADER_NV";
            break;
        default:
            std::cerr << "Unknown error (" << result << ")";
            break;
    }
    
    std::cerr << std::endl;
    return false;
}

// Debug utilities implementation
void PipelineUtils::setDebugName(VkDevice device,
                                const VulkanFunctionLoader& loader,
                                VkPipeline pipeline,
                                const std::string& name) {
    // Debug naming not yet implemented in VulkanFunctionLoader
    // TODO: Add VK_EXT_debug_utils extension support to VulkanFunctionLoader
    (void)device;
    (void)loader; 
    (void)pipeline;
    (void)name;
}