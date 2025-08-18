#include "graphics_render_pass_manager.h"
#include "hash_utils.h"
#include <iostream>

GraphicsRenderPassManager::GraphicsRenderPassManager(VulkanContext* ctx) : VulkanManagerBase(ctx) {
}

VkRenderPass GraphicsRenderPassManager::createRenderPass(VkFormat colorFormat, 
                                                        VkFormat depthFormat,
                                                        VkSampleCountFlagBits samples,
                                                        bool enableMSAA) {
    size_t hash = createRenderPassHash(colorFormat, depthFormat, samples, enableMSAA);
    
    auto it = renderPassCache_.find(hash);
    if (it != renderPassCache_.end()) {
        return it->second.get();
    }
    
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    VkAttachmentReference depthAttachmentRef{};
    VkAttachmentReference resolveAttachmentRef{};
    
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = enableMSAA ? samples : VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = enableMSAA ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = enableMSAA ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments.push_back(colorAttachment);
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRefs.push_back(colorAttachmentRef);
    
    if (enableMSAA) {
        VkAttachmentDescription resolveAttachment{};
        resolveAttachment.format = colorFormat;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(resolveAttachment);
        
        resolveAttachmentRef.attachment = 1;
        resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    
    bool hasDepth = (depthFormat != VK_FORMAT_UNDEFINED);
    if (hasDepth) {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = enableMSAA ? samples : VK_SAMPLE_COUNT_1_BIT;
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
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = hasDepth ? &depthAttachmentRef : nullptr;
    subpass.pResolveAttachments = enableMSAA ? &resolveAttachmentRef : nullptr;
    
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
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    VkRenderPass renderPass;
    VkResult result = vkCreateRenderPassWrapper(&renderPassInfo, &renderPass);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create render pass: " << result << std::endl;
        return VK_NULL_HANDLE;
    }
    
    vulkan_raii::RenderPass raiiRenderPass = vulkan_raii::make_render_pass(renderPass, context);
    VkRenderPass handle = raiiRenderPass.get();
    renderPassCache_[hash] = std::move(raiiRenderPass);
    
    return handle;
}

void GraphicsRenderPassManager::clearCache() {
    std::cout << "GraphicsRenderPassManager: Clearing render pass cache (" << renderPassCache_.size() << " render passes)" << std::endl;
    renderPassCache_.clear();
    std::cout << "GraphicsRenderPassManager: Render pass cache cleared successfully" << std::endl;
}

size_t GraphicsRenderPassManager::createRenderPassHash(VkFormat colorFormat, VkFormat depthFormat, 
                                                      VkSampleCountFlagBits samples, bool enableMSAA) const {
    return VulkanHash::hash_combine(colorFormat, depthFormat, samples, enableMSAA);
}