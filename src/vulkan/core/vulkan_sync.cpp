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
    cleanupBeforeContextDestruction();
}

void VulkanSync::cleanupBeforeContextDestruction() {
    if (context) {
        // Clear RAII wrappers before context destruction
        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        computeFinishedSemaphores.clear();
        inFlightFences.clear();
        computeFences.clear();
        
        if (commandPool != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyCommandPool(context->getDevice(), commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        
        context = nullptr;
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
    // Reserve space but don't resize (RAII wrappers don't have default constructor that we want to use)
    imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);
    computeFences.reserve(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore, computeFinishedSemaphore;
        VkFence inFlightFence, computeFence;
        
        if (context->getLoader().vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            context->getLoader().vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            context->getLoader().vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &computeFinishedSemaphore) != VK_SUCCESS ||
            context->getLoader().vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS ||
            context->getLoader().vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &computeFence) != VK_SUCCESS) {
            std::cerr << "Failed to create synchronization objects for frame " << i << std::endl;
            // Clean up any objects created so far
            imageAvailableSemaphores.clear();
            renderFinishedSemaphores.clear();
            computeFinishedSemaphores.clear();
            inFlightFences.clear();
            computeFences.clear();
            return false;
        }
        
        // Wrap in RAII wrappers
        imageAvailableSemaphores.emplace_back(imageAvailableSemaphore, context);
        renderFinishedSemaphores.emplace_back(renderFinishedSemaphore, context);
        computeFinishedSemaphores.emplace_back(computeFinishedSemaphore, context);
        inFlightFences.emplace_back(inFlightFence, context);
        computeFences.emplace_back(computeFence, context);
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
    
    // POTENTIAL FIX: Add detailed logging for command pool reset during resize
    std::cout << "VulkanSync: Attempting to reset command pool..." << std::endl;
    
    // Optimized: Use vkResetCommandPool for batch reset of all buffers
    // This is more efficient than individual resets when resetting many buffers
    VkResult result = context->getLoader().vkResetCommandPool(context->getDevice(), commandPool, 0);
    if (result != VK_SUCCESS) {
        std::cerr << "VulkanSync: CRITICAL - Command pool reset FAILED with VkResult: " << result << std::endl;
        std::cerr << "VulkanSync: This may indicate command pool corruption - falling back to individual resets" << std::endl;
        
        // Fallback to individual resets if pool reset fails
        for (size_t i = 0; i < commandBuffers.size(); ++i) {
            VkResult cmdResult = context->getLoader().vkResetCommandBuffer(commandBuffers[i], 0);
            if (cmdResult != VK_SUCCESS) {
                std::cerr << "VulkanSync: Individual graphics command buffer " << i << " reset failed: " << cmdResult << std::endl;
            }
        }
        for (size_t i = 0; i < computeCommandBuffers.size(); ++i) {
            VkResult cmdResult = context->getLoader().vkResetCommandBuffer(computeCommandBuffers[i], 0);
            if (cmdResult != VK_SUCCESS) {
                std::cerr << "VulkanSync: Individual compute command buffer " << i << " reset failed: " << cmdResult << std::endl;
            }
        }
    } else {
        std::cout << "VulkanSync: Command pool reset successful" << std::endl;
    }
}

bool VulkanSync::recreateCommandPool() {
    if (!context) {
        std::cerr << "VulkanSync: Cannot recreate command pool - no context" << std::endl;
        return false;
    }
    
    std::cout << "VulkanSync: CRITICAL FIX - Recreating command pool to fix resize corruption" << std::endl;
    
    // Wait for device to be idle before destroying command pool
    context->getLoader().vkDeviceWaitIdle(context->getDevice());
    
    // Destroy old command pool (this automatically frees all command buffers)
    if (commandPool != VK_NULL_HANDLE) {
        std::cout << "VulkanSync: Destroying corrupted command pool" << std::endl;
        context->getLoader().vkDestroyCommandPool(context->getDevice(), commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
    
    // Clear command buffer vectors (handles are now invalid)
    commandBuffers.clear();
    computeCommandBuffers.clear();
    
    // Recreate command pool
    if (!createCommandPool()) {
        std::cerr << "VulkanSync: CRITICAL FAILURE - Could not recreate command pool" << std::endl;
        return false;
    }
    
    // Recreate command buffers
    if (!createCommandBuffers()) {
        std::cerr << "VulkanSync: CRITICAL FAILURE - Could not recreate command buffers" << std::endl;
        return false;
    }
    
    std::cout << "VulkanSync: Command pool and buffers successfully recreated" << std::endl;
    return true;
}

// Individual handle getters
VkSemaphore VulkanSync::getImageAvailableSemaphore(size_t index) const {
    if (index >= imageAvailableSemaphores.size()) {
        return VK_NULL_HANDLE;
    }
    return imageAvailableSemaphores[index].get();
}

VkSemaphore VulkanSync::getRenderFinishedSemaphore(size_t index) const {
    if (index >= renderFinishedSemaphores.size()) {
        return VK_NULL_HANDLE;
    }
    return renderFinishedSemaphores[index].get();
}

VkSemaphore VulkanSync::getComputeFinishedSemaphore(size_t index) const {
    if (index >= computeFinishedSemaphores.size()) {
        return VK_NULL_HANDLE;
    }
    return computeFinishedSemaphores[index].get();
}

VkFence VulkanSync::getInFlightFence(size_t index) const {
    if (index >= inFlightFences.size()) {
        return VK_NULL_HANDLE;
    }
    return inFlightFences[index].get();
}

VkFence VulkanSync::getComputeFence(size_t index) const {
    if (index >= computeFences.size()) {
        return VK_NULL_HANDLE;
    }
    return computeFences[index].get();
}

// Getter implementations that return raw Vulkan handles
std::vector<VkSemaphore> VulkanSync::getImageAvailableSemaphores() const {
    std::vector<VkSemaphore> handles;
    handles.reserve(imageAvailableSemaphores.size());
    for (const auto& semaphore : imageAvailableSemaphores) {
        handles.push_back(semaphore.get());
    }
    return handles;
}

std::vector<VkSemaphore> VulkanSync::getRenderFinishedSemaphores() const {
    std::vector<VkSemaphore> handles;
    handles.reserve(renderFinishedSemaphores.size());
    for (const auto& semaphore : renderFinishedSemaphores) {
        handles.push_back(semaphore.get());
    }
    return handles;
}

std::vector<VkSemaphore> VulkanSync::getComputeFinishedSemaphores() const {
    std::vector<VkSemaphore> handles;
    handles.reserve(computeFinishedSemaphores.size());
    for (const auto& semaphore : computeFinishedSemaphores) {
        handles.push_back(semaphore.get());
    }
    return handles;
}

std::vector<VkFence> VulkanSync::getInFlightFences() const {
    std::vector<VkFence> handles;
    handles.reserve(inFlightFences.size());
    for (const auto& fence : inFlightFences) {
        handles.push_back(fence.get());
    }
    return handles;
}

std::vector<VkFence> VulkanSync::getComputeFences() const {
    std::vector<VkFence> handles;
    handles.reserve(computeFences.size());
    for (const auto& fence : computeFences) {
        handles.push_back(fence.get());
    }
    return handles;
}

