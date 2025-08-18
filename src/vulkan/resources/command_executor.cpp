#include "command_executor.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include <iostream>

CommandExecutor::CommandExecutor() {
}

CommandExecutor::~CommandExecutor() {
    cleanup();
}

bool CommandExecutor::initialize(const VulkanContext& context, VkCommandPool commandPool) {
    this->context = &context;
    this->commandPool = commandPool;
    
    if (commandPool == VK_NULL_HANDLE) {
        std::cerr << "CommandExecutor: Invalid command pool provided!" << std::endl;
        return false;
    }
    
    // Create dedicated transfer command pool for async operations
    if (!createTransferCommandPool()) {
        std::cerr << "CommandExecutor: Failed to create transfer command pool!" << std::endl;
        return false;
    }
    
    return true;
}

void CommandExecutor::cleanup() {
    cleanupTransferCommandPool();
    context = nullptr;
    commandPool = VK_NULL_HANDLE;
}

void CommandExecutor::copyBufferToBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, 
                                        VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!context || commandPool == VK_NULL_HANDLE) {
        std::cerr << "CommandExecutor: Not properly initialized!" << std::endl;
        return;
    }
    
    if (src == VK_NULL_HANDLE || dst == VK_NULL_HANDLE) {
        std::cerr << "CommandExecutor: Invalid buffer handles!" << std::endl;
        return;
    }
    
    if (size == 0) {
        std::cerr << "CommandExecutor: Copy size cannot be zero!" << std::endl;
        return;
    }
    
    VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(
        context->getDevice(), 
        context->getLoader(), 
        commandPool
    );
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    
    context->getLoader().vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
    
    VulkanUtils::endSingleTimeCommands(
        context->getDevice(),
        context->getLoader(),
        context->getGraphicsQueue(),
        commandPool,
        commandBuffer
    );
}

CommandExecutor::AsyncTransfer CommandExecutor::copyBufferToBufferAsync(VkBuffer src, VkBuffer dst, VkDeviceSize size,
                                                                        VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    AsyncTransfer transfer{};
    
    if (!context || transferCommandPool == VK_NULL_HANDLE) {
        std::cerr << "CommandExecutor: Not properly initialized for async transfers!" << std::endl;
        return transfer;
    }
    
    if (src == VK_NULL_HANDLE || dst == VK_NULL_HANDLE || size == 0) {
        std::cerr << "CommandExecutor: Invalid parameters for async transfer!" << std::endl;
        return transfer;
    }
    
    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool;
    allocInfo.commandBufferCount = 1;
    
    if (context->getLoader().vkAllocateCommandBuffers(context->getDevice(), &allocInfo, &transfer.commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffer for async transfer!" << std::endl;
        return transfer;
    }
    
    // Create fence for synchronization
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    if (context->getLoader().vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &transfer.fence) != VK_SUCCESS) {
        std::cerr << "Failed to create fence for async transfer!" << std::endl;
        context->getLoader().vkFreeCommandBuffers(context->getDevice(), transferCommandPool, 1, &transfer.commandBuffer);
        transfer.commandBuffer = VK_NULL_HANDLE;
        return transfer;
    }
    
    // Record command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    context->getLoader().vkBeginCommandBuffer(transfer.commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    
    context->getLoader().vkCmdCopyBuffer(transfer.commandBuffer, src, dst, 1, &copyRegion);
    context->getLoader().vkEndCommandBuffer(transfer.commandBuffer);
    
    // Submit to graphics queue (or transfer queue if available)
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transfer.commandBuffer;
    
    if (context->getLoader().vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, transfer.fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit async transfer command buffer!" << std::endl;
        freeAsyncTransfer(transfer);
        return {};
    }
    
    return transfer;
}

bool CommandExecutor::isTransferComplete(const AsyncTransfer& transfer) {
    if (transfer.fence == VK_NULL_HANDLE) {
        return true; // Invalid transfer is considered complete
    }
    
    VkResult result = context->getLoader().vkGetFenceStatus(context->getDevice(), transfer.fence);
    return result == VK_SUCCESS;
}

void CommandExecutor::waitForTransfer(const AsyncTransfer& transfer) {
    if (transfer.fence != VK_NULL_HANDLE) {
        context->getLoader().vkWaitForFences(context->getDevice(), 1, &transfer.fence, VK_TRUE, UINT64_MAX);
    }
}

void CommandExecutor::freeAsyncTransfer(AsyncTransfer& transfer) {
    if (transfer.fence != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyFence(context->getDevice(), transfer.fence, nullptr);
        transfer.fence = VK_NULL_HANDLE;
    }
    
    if (transfer.commandBuffer != VK_NULL_HANDLE) {
        context->getLoader().vkFreeCommandBuffers(context->getDevice(), transferCommandPool, 1, &transfer.commandBuffer);
        transfer.commandBuffer = VK_NULL_HANDLE;
    }
    
    transfer.completed = false;
}

bool CommandExecutor::createTransferCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->getGraphicsQueueFamily();
    
    return context->getLoader().vkCreateCommandPool(context->getDevice(), &poolInfo, nullptr, &transferCommandPool) == VK_SUCCESS;
}

void CommandExecutor::cleanupTransferCommandPool() {
    if (transferCommandPool != VK_NULL_HANDLE && context) {
        context->getLoader().vkDestroyCommandPool(context->getDevice(), transferCommandPool, nullptr);
        transferCommandPool = VK_NULL_HANDLE;
    }
}