#include "vulkan_utils.h"
#include "vulkan_function_loader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    // Helper function to convert legacy pipeline stage flags to Synchronization2
    VkPipelineStageFlags2 convertPipelineStageToSynchronization2(VkPipelineStageFlags legacyStage) {
        switch (legacyStage) {
            case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
                return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
                return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
                return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
            case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
                return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
                return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
                return VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
                return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            case VK_PIPELINE_STAGE_TRANSFER_BIT:
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
                return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            case VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT:
                return VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
            case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            default:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // Conservative fallback
        }
    }
}

uint32_t VulkanUtils::findMemoryType(VkPhysicalDevice physicalDevice, 
                                    const VulkanFunctionLoader& loader,
                                    uint32_t typeFilter, 
                                    VkMemoryPropertyFlags properties) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vk.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

bool VulkanUtils::createBuffer(VkDevice device, 
                             const VulkanFunctionLoader& loader,
                             VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer,
                             VkDeviceMemory& bufferMemory) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vk.vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer!" << std::endl;
        return false;
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    vk.vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vk.getPhysicalDevice(), vk,
                                             memRequirements.memoryTypeBits, properties);
    
    if (vk.vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory!" << std::endl;
        vk.vkDestroyBuffer(device, buffer, nullptr);
        return false;
    }
    
    vk.vkBindBufferMemory(device, buffer, bufferMemory, 0);
    return true;
}

bool VulkanUtils::createImage(VkDevice device,
                            VkPhysicalDevice physicalDevice,
                            const VulkanFunctionLoader& loader,
                            uint32_t width,
                            uint32_t height,
                            VkFormat format,
                            VkImageTiling tiling,
                            VkImageUsageFlags usage,
                            VkMemoryPropertyFlags properties,
                            VkImage& image,
                            VkDeviceMemory& imageMemory,
                            VkSampleCountFlagBits numSamples) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vk.vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "Failed to create image!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vk.vkGetImageMemoryRequirements(device, image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, vk,
                                             memRequirements.memoryTypeBits, properties);
    
    if (vk.vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate image memory!" << std::endl;
        vk.vkDestroyImage(device, image, nullptr);
        return false;
    }
    
    vk.vkBindImageMemory(device, image, imageMemory, 0);
    return true;
}

VkImageView VulkanUtils::createImageView(VkDevice device,
                                       const VulkanFunctionLoader& loader,
                                       VkImage image,
                                       VkFormat format,
                                       VkImageAspectFlags aspectFlags) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkImageView imageView;
    if (vk.vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }
    
    return imageView;
}

std::vector<char> VulkanUtils::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    
    return buffer;
}

VkShaderModule VulkanUtils::createShaderModule(VkDevice device,
                                              const VulkanFunctionLoader& loader,
                                              const std::vector<char>& code) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if (vk.vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return shaderModule;
}

VkCommandBuffer VulkanUtils::beginSingleTimeCommands(VkDevice device,
                                                    const VulkanFunctionLoader& loader,
                                                    VkCommandPool commandPool) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vk.vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vk.vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanUtils::endSingleTimeCommands(VkDevice device,
                                       const VulkanFunctionLoader& loader,
                                       VkQueue queue,
                                       VkCommandPool commandPool,
                                       VkCommandBuffer commandBuffer) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    vk.vkEndCommandBuffer(commandBuffer);
    
    // Use Synchronization2 for single-time command submission
    VkCommandBufferSubmitInfo cmdSubmitInfo{};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = commandBuffer;
    cmdSubmitInfo.deviceMask = 0;
    
    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    
    vk.vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vk.vkQueueWaitIdle(queue);
    
    // Note: Command buffer will be freed automatically when command pool is destroyed
}

void VulkanUtils::transitionImageLayout(VkDevice device,
                                      const VulkanFunctionLoader& loader,
                                      VkQueue queue,
                                      VkCommandPool commandPool,
                                      VkImage image,
                                      VkFormat format,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, vk, commandPool);
    
    // Use Synchronization2 for image layout transitions
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }
    
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;
    
    vk.vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    
    endSingleTimeCommands(device, vk, queue, commandPool, commandBuffer);
}

void VulkanUtils::copyBuffer(VkDevice device,
                           const VulkanFunctionLoader& loader,
                           VkQueue queue,
                           VkCommandPool commandPool,
                           VkBuffer srcBuffer,
                           VkBuffer dstBuffer,
                           VkDeviceSize size) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, vk, commandPool);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vk.vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    endSingleTimeCommands(device, vk, queue, commandPool, commandBuffer);
}

void VulkanUtils::copyBufferToImage(VkDevice device,
                                  const VulkanFunctionLoader& loader,
                                  VkQueue queue,
                                  VkCommandPool commandPool,
                                  VkBuffer buffer,
                                  VkImage image,
                                  uint32_t width,
                                  uint32_t height) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, vk, commandPool);
    
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vk.vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    endSingleTimeCommands(device, vk, queue, commandPool, commandBuffer);
}

void VulkanUtils::writeDescriptorSets(VkDevice device,
                                     const VulkanFunctionLoader& loader,
                                     VkDescriptorSet descriptorSet,
                                     const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                                     VkDescriptorType descriptorType) {
    // Cache loader reference for performance
    const auto& vk = loader;
    
    std::vector<VkWriteDescriptorSet> descriptorWrites(bufferInfos.size());
    
    for (size_t i = 0; i < bufferInfos.size(); i++) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = descriptorSet;
        descriptorWrites[i].dstBinding = static_cast<uint32_t>(i);
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorType = descriptorType;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pBufferInfo = &bufferInfos[i];
    }
    
    vk.vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), 
                            descriptorWrites.data(), 0, nullptr);
}

// Synchronization utilities implementation
VkFence VulkanUtils::createFence(VkDevice device, const VulkanFunctionLoader& loader, bool signaled) {
    const auto& vk = loader;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    
    VkFence fence;
    if (vk.vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return fence;
}

VkSemaphore VulkanUtils::createSemaphore(VkDevice device, const VulkanFunctionLoader& loader) {
    const auto& vk = loader;
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkSemaphore semaphore;
    if (vk.vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return semaphore;
}

void VulkanUtils::destroyFences(VkDevice device, const VulkanFunctionLoader& loader, 
                               const std::vector<VkFence>& fences) {
    const auto& vk = loader;
    
    for (VkFence fence : fences) {
        if (fence != VK_NULL_HANDLE) {
            vk.vkDestroyFence(device, fence, nullptr);
        }
    }
}

void VulkanUtils::destroySemaphores(VkDevice device, const VulkanFunctionLoader& loader,
                                  const std::vector<VkSemaphore>& semaphores) {
    const auto& vk = loader;
    
    for (VkSemaphore semaphore : semaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vk.vkDestroySemaphore(device, semaphore, nullptr);
        }
    }
}

VkResult VulkanUtils::waitForFences(VkDevice device, const VulkanFunctionLoader& loader,
                                  const std::vector<VkFence>& fences, bool waitAll, uint64_t timeout) {
    if (fences.empty()) {
        return VK_SUCCESS;
    }
    
    const auto& vk = loader;
    
    return vk.vkWaitForFences(device, static_cast<uint32_t>(fences.size()), fences.data(), 
                             waitAll ? VK_TRUE : VK_FALSE, timeout);
}

// Command buffer utilities implementation
VkResult VulkanUtils::submitCommands(VkQueue queue, const VulkanFunctionLoader& loader,
                                   const std::vector<VkCommandBuffer>& commandBuffers,
                                   const std::vector<VkSemaphore>& waitSemaphores,
                                   const std::vector<VkPipelineStageFlags>& waitStages,
                                   const std::vector<VkSemaphore>& signalSemaphores,
                                   VkFence fence) {
    if (commandBuffers.empty()) {
        return VK_SUCCESS;
    }
    
    const auto& vk = loader;
    
    // Convert to Synchronization2 structures
    std::vector<VkCommandBufferSubmitInfo> cmdSubmitInfos(commandBuffers.size());
    for (size_t i = 0; i < commandBuffers.size(); ++i) {
        cmdSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdSubmitInfos[i].commandBuffer = commandBuffers[i];
        cmdSubmitInfos[i].deviceMask = 0;
    }
    
    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreInfos(waitSemaphores.size());
    for (size_t i = 0; i < waitSemaphores.size(); ++i) {
        waitSemaphoreInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfos[i].semaphore = waitSemaphores[i];
        // Convert legacy pipeline stage flags to Synchronization2
        waitSemaphoreInfos[i].stageMask = (i < waitStages.size()) ? 
            convertPipelineStageToSynchronization2(waitStages[i]) : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        waitSemaphoreInfos[i].deviceIndex = 0;
    }
    
    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreInfos(signalSemaphores.size());
    for (size_t i = 0; i < signalSemaphores.size(); ++i) {
        signalSemaphoreInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfos[i].semaphore = signalSemaphores[i];
        signalSemaphoreInfos[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // Conservative for compatibility
        signalSemaphoreInfos[i].deviceIndex = 0;
    }
    
    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = static_cast<uint32_t>(cmdSubmitInfos.size());
    submitInfo.pCommandBufferInfos = cmdSubmitInfos.data();
    
    if (!waitSemaphoreInfos.empty()) {
        submitInfo.waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfos.size());
        submitInfo.pWaitSemaphoreInfos = waitSemaphoreInfos.data();
    }
    
    if (!signalSemaphoreInfos.empty()) {
        submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size());
        submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfos.data();
    }
    
    return vk.vkQueueSubmit2(queue, 1, &submitInfo, fence);
}

VkResult VulkanUtils::allocateCommandBuffers(VkDevice device, const VulkanFunctionLoader& loader,
                                           VkCommandPool commandPool, uint32_t commandBufferCount,
                                           std::vector<VkCommandBuffer>& commandBuffers,
                                           VkCommandBufferLevel level) {
    const auto& vk = loader;
    
    commandBuffers.resize(commandBufferCount);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = commandBufferCount;
    
    return vk.vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
}

// Structure creation helpers implementation
VkSubmitInfo VulkanUtils::createSubmitInfo(const std::vector<VkCommandBuffer>& commandBuffers,
                                         const std::vector<VkSemaphore>& waitSemaphores,
                                         const std::vector<VkPipelineStageFlags>& waitStages,
                                         const std::vector<VkSemaphore>& signalSemaphores) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    if (!waitSemaphores.empty()) {
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        
        if (!waitStages.empty()) {
            submitInfo.pWaitDstStageMask = waitStages.data();
        }
    }
    
    if (!commandBuffers.empty()) {
        submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.data();
    }
    
    if (!signalSemaphores.empty()) {
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.data();
    }
    
    return submitInfo;
}

VkPresentInfoKHR VulkanUtils::createPresentInfo(const std::vector<VkSwapchainKHR>& swapchains,
                                               const std::vector<uint32_t>& imageIndices,
                                               const std::vector<VkSemaphore>& waitSemaphores) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    
    if (!waitSemaphores.empty()) {
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        presentInfo.pWaitSemaphores = waitSemaphores.data();
    }
    
    if (!swapchains.empty()) {
        presentInfo.swapchainCount = static_cast<uint32_t>(swapchains.size());
        presentInfo.pSwapchains = swapchains.data();
        presentInfo.pImageIndices = imageIndices.data();
    }
    
    return presentInfo;
}

// Error handling utilities implementation
bool VulkanUtils::checkVkResult(VkResult result, const char* operation) {
    if (result == VK_SUCCESS) {
        return true;
    }
    
    logVkResult(result, operation);
    return false;
}

void VulkanUtils::logVkResult(VkResult result, const char* operation) {
    const char* resultString = "Unknown";
    
    switch (result) {
        case VK_SUCCESS: resultString = "VK_SUCCESS"; break;
        case VK_NOT_READY: resultString = "VK_NOT_READY"; break;
        case VK_TIMEOUT: resultString = "VK_TIMEOUT"; break;
        case VK_EVENT_SET: resultString = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET: resultString = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE: resultString = "VK_INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: resultString = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: resultString = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: resultString = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST: resultString = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: resultString = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT: resultString = "VK_ERROR_LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: resultString = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_FEATURE_NOT_PRESENT: resultString = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: resultString = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_TOO_MANY_OBJECTS: resultString = "VK_ERROR_TOO_MANY_OBJECTS"; break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED: resultString = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
        case VK_ERROR_SURFACE_LOST_KHR: resultString = "VK_ERROR_SURFACE_LOST_KHR"; break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: resultString = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
        case VK_SUBOPTIMAL_KHR: resultString = "VK_SUBOPTIMAL_KHR"; break;
        case VK_ERROR_OUT_OF_DATE_KHR: resultString = "VK_ERROR_OUT_OF_DATE_KHR"; break;
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: resultString = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
        case VK_ERROR_VALIDATION_FAILED_EXT: resultString = "VK_ERROR_VALIDATION_FAILED_EXT"; break;
        default: break;
    }
    
    std::cerr << "VulkanUtils: " << operation << " failed with " << resultString 
              << " (" << result << ")" << std::endl;
}