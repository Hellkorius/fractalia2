#include "graphics_pipeline_state_hash.h"
#include "hash_utils.h"

bool GraphicsPipelineState::operator==(const GraphicsPipelineState& other) const {
    if (pushConstantRanges.size() != other.pushConstantRanges.size()) {
        return false;
    }
    for (size_t i = 0; i < pushConstantRanges.size(); ++i) {
        const auto& a = pushConstantRanges[i];
        const auto& b = other.pushConstantRanges[i];
        if (a.stageFlags != b.stageFlags || a.offset != b.offset || a.size != b.size) {
            return false;
        }
    }
    
    if (vertexBindings.size() != other.vertexBindings.size()) {
        return false;
    }
    for (size_t i = 0; i < vertexBindings.size(); ++i) {
        const auto& a = vertexBindings[i];
        const auto& b = other.vertexBindings[i];
        if (a.binding != b.binding || a.stride != b.stride || a.inputRate != b.inputRate) {
            return false;
        }
    }
    
    if (vertexAttributes.size() != other.vertexAttributes.size()) {
        return false;
    }
    for (size_t i = 0; i < vertexAttributes.size(); ++i) {
        const auto& a = vertexAttributes[i];
        const auto& b = other.vertexAttributes[i];
        if (a.location != b.location || a.binding != b.binding || a.format != b.format || a.offset != b.offset) {
            return false;
        }
    }
    
    if (colorBlendAttachments.size() != other.colorBlendAttachments.size()) {
        return false;
    }
    for (size_t i = 0; i < colorBlendAttachments.size(); ++i) {
        const auto& a = colorBlendAttachments[i];
        const auto& b = other.colorBlendAttachments[i];
        if (a.colorWriteMask != b.colorWriteMask || a.blendEnable != b.blendEnable ||
            a.srcColorBlendFactor != b.srcColorBlendFactor || a.dstColorBlendFactor != b.dstColorBlendFactor ||
            a.colorBlendOp != b.colorBlendOp || a.srcAlphaBlendFactor != b.srcAlphaBlendFactor ||
            a.dstAlphaBlendFactor != b.dstAlphaBlendFactor || a.alphaBlendOp != b.alphaBlendOp) {
            return false;
        }
    }
    
    return shaderStages == other.shaderStages &&
           topology == other.topology &&
           primitiveRestartEnable == other.primitiveRestartEnable &&
           polygonMode == other.polygonMode &&
           cullMode == other.cullMode &&
           frontFace == other.frontFace &&
           rasterizationSamples == other.rasterizationSamples &&
           renderPass == other.renderPass &&
           subpass == other.subpass &&
           descriptorSetLayouts == other.descriptorSetLayouts;
}

size_t GraphicsPipelineState::getHash() const {
    VulkanHash::HashCombiner hasher;
    
    hasher.combineContainer(shaderStages)
          .combine(topology)
          .combine(polygonMode)
          .combine(cullMode)
          .combine(rasterizationSamples)
          .combine(renderPass)
          .combine(subpass);
    
    for (const auto& binding : vertexBindings) {
        hasher.combine(binding.binding)
              .combine(binding.stride)
              .combine(binding.inputRate);
    }
    
    for (const auto& attr : vertexAttributes) {
        hasher.combine(attr.location)
              .combine(attr.binding)
              .combine(attr.format)
              .combine(attr.offset);
    }
    
    return hasher.get();
}

bool GraphicsPipelineStateHash::compareStates(const GraphicsPipelineState& a, const GraphicsPipelineState& b) {
    return a == b;
}

size_t GraphicsPipelineStateHash::hashPushConstantRanges(const std::vector<VkPushConstantRange>& ranges) {
    VulkanHash::HashCombiner hasher;
    for (const auto& range : ranges) {
        hasher.combine(range.stageFlags)
              .combine(range.offset)
              .combine(range.size);
    }
    return hasher.get();
}

size_t GraphicsPipelineStateHash::hashVertexBindings(const std::vector<VkVertexInputBindingDescription>& bindings) {
    VulkanHash::HashCombiner hasher;
    for (const auto& binding : bindings) {
        hasher.combine(binding.binding)
              .combine(binding.stride)
              .combine(binding.inputRate);
    }
    return hasher.get();
}

size_t GraphicsPipelineStateHash::hashVertexAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes) {
    VulkanHash::HashCombiner hasher;
    for (const auto& attr : attributes) {
        hasher.combine(attr.location)
              .combine(attr.binding)
              .combine(attr.format)
              .combine(attr.offset);
    }
    return hasher.get();
}

size_t GraphicsPipelineStateHash::hashColorBlendAttachments(const std::vector<VkPipelineColorBlendAttachmentState>& attachments) {
    VulkanHash::HashCombiner hasher;
    for (const auto& attachment : attachments) {
        hasher.combine(attachment.colorWriteMask)
              .combine(attachment.blendEnable)
              .combine(attachment.srcColorBlendFactor)
              .combine(attachment.dstColorBlendFactor)
              .combine(attachment.colorBlendOp)
              .combine(attachment.srcAlphaBlendFactor)
              .combine(attachment.dstAlphaBlendFactor)
              .combine(attachment.alphaBlendOp);
    }
    return hasher.get();
}