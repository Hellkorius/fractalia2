#include "vulkan_sync.h"
#include <iostream>

VulkanSync::VulkanSync() {
}

VulkanSync::~VulkanSync() {
    cleanup();
}

bool VulkanSync::initialize(const VulkanContext& context) {
    this->context = &context;
    
    if (!createCommandPool()) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }
    
    if (!createCommandBuffers()) {
        std::cerr << "Failed to create command buffers" << std::endl;
        return false;
    }
    
    if (!createSyncObjects()) {
        std::cerr << "Failed to create sync objects" << std::endl;
        return false;
    }
    
    return true;
}

void VulkanSync::cleanup() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (i < inFlightFences.size() && inFlightFences[i] != VK_NULL_HANDLE && context) {
            context->getLoader().vkDestroyFence(context->getDevice(), inFlightFences[i], nullptr);
        }
        if (i < computeFences.size() && computeFences[i] != VK_NULL_HANDLE && context) {
            context->getLoader().vkDestroyFence(context->getDevice(), computeFences[i], nullptr);
        }
        if (i < renderFinishedSemaphores.size() && renderFinishedSemaphores[i] != VK_NULL_HANDLE && context) {
            context->getLoader().vkDestroySemaphore(context->getDevice(), renderFinishedSemaphores[i], nullptr);
        }
        if (i < computeFinishedSemaphores.size() && computeFinishedSemaphores[i] != VK_NULL_HANDLE && context) {
            context->getLoader().vkDestroySemaphore(context->getDevice(), computeFinishedSemaphores[i], nullptr);
        }
        if (i < imageAvailableSemaphores.size() && imageAvailableSemaphores[i] != VK_NULL_HANDLE && context) {
            context->getLoader().vkDestroySemaphore(context->getDevice(), imageAvailableSemaphores[i], nullptr);
        }
    }
    
    if (commandPool != VK_NULL_HANDLE && context) {
        context->getLoader().vkDestroyCommandPool(context->getDevice(), commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
}

bool VulkanSync::createCommandPool() {
    std::cout << "VulkanSync: About to call findQueueFamilies..." << std::endl;
    QueueFamilyIndices queueFamilyIndices = context->findQueueFamilies(context->getPhysicalDevice());
    std::cout << "VulkanSync: findQueueFamilies returned" << std::endl;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (context->getLoader().vkCreateCommandPool(context->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }

    return true;
}

bool VulkanSync::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (context->getLoader().vkAllocateCommandBuffers(context->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate graphics command buffers" << std::endl;
        return false;
    }
    
    allocInfo.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers.size());
    if (context->getLoader().vkAllocateCommandBuffers(context->getDevice(), &allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate compute command buffers" << std::endl;
        return false;
    }

    return true;
}

bool VulkanSync::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (context->getLoader().vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            context->getLoader().vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            context->getLoader().vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS ||
            context->getLoader().vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS ||
            context->getLoader().vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create synchronization objects" << std::endl;
            return false;
        }
    }

    return true;
}

void VulkanSync::resetCommandBuffersForFrame(uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || !context) return;
    
    // Reset both graphics and compute command buffers for this frame
    if (frameIndex < commandBuffers.size()) {
        context->getLoader().vkResetCommandBuffer(commandBuffers[frameIndex], 0);
    }
    if (frameIndex < computeCommandBuffers.size()) {
        context->getLoader().vkResetCommandBuffer(computeCommandBuffers[frameIndex], 0);
    }
}

void VulkanSync::resetAllCommandBuffers() {
    if (!context) return;
    
    // Optimized: Use vkResetCommandPool for batch reset of all buffers
    // This is more efficient than individual resets when resetting many buffers
    VkResult result = context->getLoader().vkResetCommandPool(context->getDevice(), commandPool, 0);
    if (result != VK_SUCCESS) {
        std::cerr << "VulkanSync: Failed to reset command pool, falling back to individual resets" << std::endl;
        
        // Fallback to individual resets if pool reset fails
        for (size_t i = 0; i < commandBuffers.size(); ++i) {
            context->getLoader().vkResetCommandBuffer(commandBuffers[i], 0);
        }
        for (size_t i = 0; i < computeCommandBuffers.size(); ++i) {
            context->getLoader().vkResetCommandBuffer(computeCommandBuffers[i], 0);
        }
    }
}

