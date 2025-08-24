#include "descriptor_update_helper.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include <iostream>

bool DescriptorUpdateHelper::updateDescriptorSet(
    const VulkanContext& context,
    VkDescriptorSet descriptorSet,
    const std::vector<BufferBinding>& bindings) {
    
    if (!validateDescriptorSet(descriptorSet)) {
        return false;
    }
    
    if (bindings.empty()) {
        std::cerr << "DescriptorUpdateHelper: No bindings provided" << std::endl;
        return false;
    }

    // Validate all bindings first
    for (const auto& binding : bindings) {
        if (!validateBinding(binding)) {
            return false;
        }
    }

    // Prepare descriptor writes
    std::vector<VkWriteDescriptorSet> descriptorWrites(bindings.size());
    std::vector<VkDescriptorBufferInfo> bufferInfos(bindings.size());

    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto& binding = bindings[i];
        
        // Fill buffer info
        bufferInfos[i].buffer = binding.buffer;
        bufferInfos[i].offset = binding.offset;
        bufferInfos[i].range = binding.range;

        // Fill write descriptor
        descriptorWrites[i] = {};
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = descriptorSet;
        descriptorWrites[i].dstBinding = binding.binding;
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorType = binding.type;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pBufferInfo = &bufferInfos[i];
    }

    // Execute update
    const auto& vk = context.getLoader();
    vk.vkUpdateDescriptorSets(
        context.getDevice(), 
        static_cast<uint32_t>(descriptorWrites.size()), 
        descriptorWrites.data(), 
        0, 
        nullptr
    );

    return true;
}

bool DescriptorUpdateHelper::updateDescriptorSetWithBufferArray(
    const VulkanContext& context,
    VkDescriptorSet descriptorSet,
    uint32_t binding,
    const std::vector<VkBuffer>& buffers,
    VkDescriptorType descriptorType,
    VkDeviceSize bufferSize) {
    
    if (!validateDescriptorSet(descriptorSet)) {
        return false;
    }
    
    if (buffers.empty()) {
        std::cerr << "DescriptorUpdateHelper: No buffers provided for array binding" << std::endl;
        return false;
    }
    
    // Validate all buffers
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i] == VK_NULL_HANDLE) {
            std::cerr << "DescriptorUpdateHelper: Buffer " << i << " is VK_NULL_HANDLE in array binding" << std::endl;
            return false;
        }
    }
    
    // Prepare buffer infos for array
    std::vector<VkDescriptorBufferInfo> bufferInfos(buffers.size());
    for (size_t i = 0; i < buffers.size(); ++i) {
        bufferInfos[i].buffer = buffers[i];
        bufferInfos[i].offset = 0;
        bufferInfos[i].range = bufferSize;
    }
    
    // Create write descriptor for array
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = descriptorType;
    descriptorWrite.descriptorCount = static_cast<uint32_t>(buffers.size());
    descriptorWrite.pBufferInfo = bufferInfos.data();
    
    // Execute update
    const auto& vk = context.getLoader();
    vk.vkUpdateDescriptorSets(
        context.getDevice(), 
        1, 
        &descriptorWrite, 
        0, 
        nullptr
    );
    
    return true;
}

bool DescriptorUpdateHelper::validateBinding(const BufferBinding& binding) {
    if (binding.buffer == VK_NULL_HANDLE) {
        std::cerr << "DescriptorUpdateHelper: Buffer is VK_NULL_HANDLE for binding " << binding.binding << std::endl;
        return false;
    }
    
    // Validate descriptor type
    switch (binding.type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            // Valid buffer types
            break;
        default:
            std::cerr << "DescriptorUpdateHelper: Unsupported descriptor type " << binding.type << " for binding " << binding.binding << std::endl;
            return false;
    }
    
    return true;
}

bool DescriptorUpdateHelper::validateDescriptorSet(VkDescriptorSet descriptorSet) {
    if (descriptorSet == VK_NULL_HANDLE) {
        std::cerr << "DescriptorUpdateHelper: Descriptor set is VK_NULL_HANDLE" << std::endl;
        return false;
    }
    return true;
}