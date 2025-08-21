#include "command_executor.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include "../../core/vulkan_utils.h"
#include <iostream>

CommandExecutor::CommandExecutor() {
}

CommandExecutor::~CommandExecutor() {
    cleanup();
}

bool CommandExecutor::initialize(const VulkanContext& context, QueueManager* queueManager) {
    this->context = &context;
    this->queueManager = queueManager;
    
    if (!queueManager) {
        std::cerr << "CommandExecutor: QueueManager is required for initialization!" << std::endl;
        return false;
    }
    
    std::cout << "CommandExecutor: Initialized with modern QueueManager" << std::endl;
    std::cout << "  - Using dedicated transfer queue: " << (usesDedicatedTransferQueue() ? "YES" : "NO") << std::endl;
    
    return true;
}

void CommandExecutor::cleanup() {
    cleanupBeforeContextDestruction();
    context = nullptr;
    queueManager = nullptr;
}

void CommandExecutor::cleanupBeforeContextDestruction() {
    // QueueManager handles cleanup of its own resources
    // No manual cleanup needed here
}

void CommandExecutor::copyBufferToBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, 
                                        VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!context || !queueManager) {
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
    
    // Use graphics command pool for synchronous operations
    VkCommandPool commandPool = queueManager->getCommandPool(CommandPoolType::Graphics);
    
    if (commandPool == VK_NULL_HANDLE) {
        std::cerr << "CommandExecutor: No valid graphics command pool available!" << std::endl;
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
    
    // Use graphics queue for synchronous completion
    VulkanUtils::endSingleTimeCommands(
        context->getDevice(),
        context->getLoader(),
        queueManager->getGraphicsQueue(),
        commandPool,
        commandBuffer
    );
}

CommandExecutor::AsyncTransfer CommandExecutor::copyBufferToBufferAsync(VkBuffer src, VkBuffer dst, VkDeviceSize size,
                                                                        VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!context || !queueManager) {
        std::cerr << "CommandExecutor: Not properly initialized for async transfers!" << std::endl;
        return {};
    }
    
    if (src == VK_NULL_HANDLE || dst == VK_NULL_HANDLE || size == 0) {
        std::cerr << "CommandExecutor: Invalid parameters for async transfer!" << std::endl;
        return {};
    }
    
    // Allocate transfer command from QueueManager
    AsyncTransfer transfer = queueManager->allocateTransferCommand();
    if (!transfer.isValid()) {
        std::cerr << "CommandExecutor: Failed to allocate transfer command from QueueManager!" << std::endl;
        return {};
    }
    
    // Record command buffer
    const auto& vk = context->getLoader();
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vk.vkBeginCommandBuffer(transfer.commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "CommandExecutor: Failed to begin transfer command buffer!" << std::endl;
        queueManager->freeTransferCommand(transfer);
        return {};
    }
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    
    vk.vkCmdCopyBuffer(transfer.commandBuffer, src, dst, 1, &copyRegion);
    
    if (vk.vkEndCommandBuffer(transfer.commandBuffer) != VK_SUCCESS) {
        std::cerr << "CommandExecutor: Failed to end transfer command buffer!" << std::endl;
        queueManager->freeTransferCommand(transfer);
        return {};
    }
    
    // Submit to optimal transfer queue (dedicated transfer or graphics fallback)
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transfer.commandBuffer;
    
    VkQueue transferQueue = queueManager->getTransferQueue();
    if (vk.vkQueueSubmit(transferQueue, 1, &submitInfo, transfer.fence.get()) != VK_SUCCESS) {
        std::cerr << "CommandExecutor: Failed to submit async transfer command buffer!" << std::endl;
        queueManager->freeTransferCommand(transfer);
        return {};
    }
    
    return transfer;
}

bool CommandExecutor::isTransferComplete(const AsyncTransfer& transfer) {
    if (!queueManager) {
        return true; // Invalid state is considered complete
    }
    return queueManager->isTransferComplete(transfer);
}

void CommandExecutor::waitForTransfer(const AsyncTransfer& transfer) {
    if (queueManager) {
        queueManager->waitForTransfer(transfer);
    }
}

void CommandExecutor::freeAsyncTransfer(AsyncTransfer& transfer) {
    if (queueManager) {
        queueManager->freeTransferCommand(transfer);
    }
}

bool CommandExecutor::usesDedicatedTransferQueue() const {
    return queueManager ? queueManager->hasDedicatedTransferQueue() : false;
}

VkQueue CommandExecutor::getTransferQueue() const {
    return queueManager ? queueManager->getTransferQueue() : VK_NULL_HANDLE;
}

uint32_t CommandExecutor::getTransferQueueFamily() const {
    return queueManager ? queueManager->getTransferQueueFamily() : 0;
}