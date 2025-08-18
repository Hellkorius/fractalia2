#include "resource_utils.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include <iostream>

// Descriptor pool utilities implementation
VkDescriptorPool ResourceUtils::createDescriptorPool(VkDevice device,
                                                    const VulkanFunctionLoader& loader,
                                                    const std::vector<VkDescriptorPoolSize>& poolSizes,
                                                    uint32_t maxSets,
                                                    VkDescriptorPoolCreateFlags flags) {
    const auto& vk = loader;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = flags;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    
    VkDescriptorPool descriptorPool;
    if (vk.vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return descriptorPool;
}

VkDescriptorPool ResourceUtils::createVariableDescriptorPool(VkDevice device,
                                                           const VulkanFunctionLoader& loader,
                                                           uint32_t uniformBuffers,
                                                           uint32_t storageBuffers,
                                                           uint32_t combinedImageSamplers,
                                                           uint32_t storageImages,
                                                           uint32_t maxSets) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    if (uniformBuffers > 0) {
        poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffers));
    }
    if (storageBuffers > 0) {
        poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBuffers));
    }
    if (combinedImageSamplers > 0) {
        poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, combinedImageSamplers));
    }
    if (storageImages > 0) {
        poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storageImages));
    }
    
    return createDescriptorPool(device, loader, poolSizes, maxSets);
}

// Descriptor set utilities implementation
VkResult ResourceUtils::allocateDescriptorSet(VkDevice device,
                                             const VulkanFunctionLoader& loader,
                                             VkDescriptorPool pool,
                                             VkDescriptorSetLayout layout,
                                             VkDescriptorSet& descriptorSet) {
    const auto& vk = loader;
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    
    return vk.vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
}

VkResult ResourceUtils::allocateDescriptorSets(VkDevice device,
                                              const VulkanFunctionLoader& loader,
                                              VkDescriptorPool pool,
                                              const std::vector<VkDescriptorSetLayout>& layouts,
                                              std::vector<VkDescriptorSet>& descriptorSets) {
    const auto& vk = loader;
    
    descriptorSets.resize(layouts.size());
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();
    
    return vk.vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());
}

// Descriptor update utilities implementation
VkWriteDescriptorSet ResourceUtils::createBufferWrite(VkDescriptorSet dstSet,
                                                     uint32_t binding,
                                                     VkDescriptorType type,
                                                     const VkDescriptorBufferInfo* bufferInfo,
                                                     uint32_t descriptorCount) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = dstSet;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = descriptorCount;
    write.pBufferInfo = bufferInfo;
    
    return write;
}

VkWriteDescriptorSet ResourceUtils::createImageWrite(VkDescriptorSet dstSet,
                                                   uint32_t binding,
                                                   VkDescriptorType type,
                                                   const VkDescriptorImageInfo* imageInfo,
                                                   uint32_t descriptorCount) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = dstSet;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = descriptorCount;
    write.pImageInfo = imageInfo;
    
    return write;
}

void ResourceUtils::updateDescriptorSets(VkDevice device,
                                        const VulkanFunctionLoader& loader,
                                        const std::vector<VkWriteDescriptorSet>& writes) {
    const auto& vk = loader;
    
    vk.vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// Buffer creation utilities implementation
VkResult ResourceUtils::createStagingBuffer(VkDevice device,
                                           VkPhysicalDevice physicalDevice,
                                           const VulkanFunctionLoader& loader,
                                           VkDeviceSize size,
                                           VkBuffer& buffer,
                                           VkDeviceMemory& memory,
                                           void** mappedData) {
    VkBufferUsageFlags usage = getCommonStagingBufferUsage();
    VkMemoryPropertyFlags properties = getHostVisibleMemoryProperties();
    
    if (!VulkanUtils::createBuffer(device, loader, size, usage, properties, buffer, memory)) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    
    if (mappedData != nullptr) {
        return mapBufferMemory(device, loader, memory, 0, size, mappedData);
    }
    
    return VK_SUCCESS;
}

VkResult ResourceUtils::createUniformBuffer(VkDevice device,
                                           VkPhysicalDevice physicalDevice,
                                           const VulkanFunctionLoader& loader,
                                           VkDeviceSize size,
                                           VkBuffer& buffer,
                                           VkDeviceMemory& memory) {
    VkBufferUsageFlags usage = getCommonUniformBufferUsage();
    VkMemoryPropertyFlags properties = getHostVisibleMemoryProperties();
    
    if (!VulkanUtils::createBuffer(device, loader, size, usage, properties, buffer, memory)) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    
    return VK_SUCCESS;
}

VkResult ResourceUtils::createStorageBuffer(VkDevice device,
                                           VkPhysicalDevice physicalDevice,
                                           const VulkanFunctionLoader& loader,
                                           VkDeviceSize size,
                                           VkBufferUsageFlags additionalUsage,
                                           VkBuffer& buffer,
                                           VkDeviceMemory& memory) {
    VkBufferUsageFlags usage = getCommonStorageBufferUsage() | additionalUsage;
    VkMemoryPropertyFlags properties = getDeviceLocalMemoryProperties();
    
    if (!VulkanUtils::createBuffer(device, loader, size, usage, properties, buffer, memory)) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    
    return VK_SUCCESS;
}

// Memory mapping utilities implementation
VkResult ResourceUtils::mapBufferMemory(VkDevice device,
                                       const VulkanFunctionLoader& loader,
                                       VkDeviceMemory memory,
                                       VkDeviceSize offset,
                                       VkDeviceSize size,
                                       void** data) {
    const auto& vk = loader;
    
    return vk.vkMapMemory(device, memory, offset, size, 0, data);
}

void ResourceUtils::unmapBufferMemory(VkDevice device,
                                     const VulkanFunctionLoader& loader,
                                     VkDeviceMemory memory) {
    const auto& vk = loader;
    
    vk.vkUnmapMemory(device, memory);
}

VkResult ResourceUtils::flushBufferMemory(VkDevice device,
                                         const VulkanFunctionLoader& loader,
                                         VkDeviceMemory memory,
                                         VkDeviceSize offset,
                                         VkDeviceSize size) {
    // vkFlushMappedMemoryRanges not yet loaded in VulkanFunctionLoader
    // For now, assume coherent memory is used
    // TODO: Add vkFlushMappedMemoryRanges to VulkanFunctionLoader
    (void)device;
    (void)loader;
    (void)memory;
    (void)offset;
    (void)size;
    
    return VK_SUCCESS; // Coherent memory doesn't need explicit flushing
}

// Resource cleanup utilities implementation
void ResourceUtils::destroyBuffer(VkDevice device,
                                 const VulkanFunctionLoader& loader,
                                 VkBuffer buffer,
                                 VkDeviceMemory memory) {
    const auto& vk = loader;
    
    if (buffer != VK_NULL_HANDLE) {
        vk.vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vk.vkFreeMemory(device, memory, nullptr);
    }
}

void ResourceUtils::destroyDescriptorPool(VkDevice device,
                                         const VulkanFunctionLoader& loader,
                                         VkDescriptorPool pool) {
    const auto& vk = loader;
    
    if (pool != VK_NULL_HANDLE) {
        vk.vkDestroyDescriptorPool(device, pool, nullptr);
    }
}

// Descriptor pool size calculation utilities implementation
std::vector<VkDescriptorPoolSize> ResourceUtils::calculatePoolSizes(
    const std::vector<VkDescriptorSetLayout>& layouts,
    const VulkanFunctionLoader& loader,
    uint32_t maxSets) {
    
    // This is a simplified implementation - in practice, you'd need to reflect
    // on the descriptor set layouts to determine actual usage
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    // Default conservative estimates
    poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets * 2));
    poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxSets * 4));
    poolSizes.push_back(createPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * 2));
    
    return poolSizes;
}

VkDescriptorPoolSize ResourceUtils::createPoolSize(VkDescriptorType type, uint32_t descriptorCount) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = type;
    poolSize.descriptorCount = descriptorCount;
    
    return poolSize;
}

// Buffer usage helper utilities implementation
VkBufferUsageFlags ResourceUtils::getCommonStorageBufferUsage() {
    return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
           VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
}

VkBufferUsageFlags ResourceUtils::getCommonUniformBufferUsage() {
    return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
}

VkBufferUsageFlags ResourceUtils::getCommonStagingBufferUsage() {
    return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | 
           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
}

// Memory property helpers implementation
VkMemoryPropertyFlags ResourceUtils::getHostVisibleMemoryProperties() {
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

VkMemoryPropertyFlags ResourceUtils::getDeviceLocalMemoryProperties() {
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

VkMemoryPropertyFlags ResourceUtils::getHostCoherentMemoryProperties() {
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | 
           VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
}

// Error handling implementation
bool ResourceUtils::checkDescriptorResult(VkResult result, const char* operation) {
    if (result == VK_SUCCESS) {
        return true;
    }
    
    std::cerr << "ResourceUtils: Descriptor " << operation << " failed: ";
    
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_FRAGMENTED_POOL:
            std::cerr << "VK_ERROR_FRAGMENTED_POOL";
            break;
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_POOL_MEMORY";
            break;
        default:
            std::cerr << "Unknown error (" << result << ")";
            break;
    }
    
    std::cerr << std::endl;
    return false;
}

bool ResourceUtils::checkBufferResult(VkResult result, const char* operation) {
    if (result == VK_SUCCESS) {
        return true;
    }
    
    std::cerr << "ResourceUtils: Buffer " << operation << " failed: ";
    
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cerr << "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_MEMORY_MAP_FAILED:
            std::cerr << "VK_ERROR_MEMORY_MAP_FAILED";
            break;
        default:
            std::cerr << "Unknown error (" << result << ")";
            break;
    }
    
    std::cerr << std::endl;
    return false;
}

// Debug utilities implementation
void ResourceUtils::setBufferDebugName(VkDevice device,
                                      const VulkanFunctionLoader& loader,
                                      VkBuffer buffer,
                                      const std::string& name) {
    // Debug naming not yet implemented in VulkanFunctionLoader
    // TODO: Add VK_EXT_debug_utils extension support to VulkanFunctionLoader
    (void)device;
    (void)loader;
    (void)buffer;
    (void)name;
}

void ResourceUtils::setDescriptorSetDebugName(VkDevice device,
                                             const VulkanFunctionLoader& loader,
                                             VkDescriptorSet descriptorSet,
                                             const std::string& name) {
    // Debug naming not yet implemented in VulkanFunctionLoader
    // TODO: Add VK_EXT_debug_utils extension support to VulkanFunctionLoader
    (void)device;
    (void)loader;
    (void)descriptorSet;
    (void)name;
}