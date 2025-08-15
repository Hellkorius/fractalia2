#include "command_executor.h"
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include "vulkan_utils.h"
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
    
    return true;
}

void CommandExecutor::cleanup() {
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