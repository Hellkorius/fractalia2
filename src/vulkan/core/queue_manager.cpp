#include "queue_manager.h"
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include <iostream>
#include <algorithm>

QueueManager::QueueManager() {
}

QueueManager::~QueueManager() {
    cleanup();
}

bool QueueManager::initialize(const VulkanContext& context) {
    this->context = &context;
    
    if (!context.getDevice() || !context.getQueueFamilyIndices().isComplete()) {
        std::cerr << "QueueManager: Invalid VulkanContext provided" << std::endl;
        return false;
    }
    
    if (!createCommandPools()) {
        std::cerr << "QueueManager: Failed to create command pools" << std::endl;
        return false;
    }
    
    if (!createFrameCommandBuffers()) {
        std::cerr << "QueueManager: Failed to create frame command buffers" << std::endl;
        return false;
    }
    
    std::cout << "QueueManager: Initialized with capabilities:" << std::endl;
    std::cout << "  - Dedicated compute queue: " << (hasDedicatedComputeQueue() ? "YES" : "NO") << std::endl;
    std::cout << "  - Dedicated transfer queue: " << (hasDedicatedTransferQueue() ? "YES" : "NO") << std::endl;
    std::cout << "  - Async compute support: " << (supportsAsyncCompute() ? "YES" : "NO") << std::endl;
    
    return true;
}

void QueueManager::cleanup() {
    cleanupBeforeContextDestruction();
    context = nullptr;
}

void QueueManager::cleanupBeforeContextDestruction() {
    // RAII command pools will clean up automatically
    graphicsCommandPool.reset();
    computeCommandPool.reset();
    transferCommandPool.reset();
    
    // Command buffer handles are freed when pools are destroyed
    graphicsCommandBuffers.clear();
    computeCommandBuffers.clear();
}

VkQueue QueueManager::getGraphicsQueue() const {
    return context ? context->getGraphicsQueue() : VK_NULL_HANDLE;
}

VkQueue QueueManager::getComputeQueue() const {
    return context ? context->getComputeQueue() : VK_NULL_HANDLE;
}

VkQueue QueueManager::getTransferQueue() const {
    return context ? context->getTransferQueue() : VK_NULL_HANDLE;
}

VkQueue QueueManager::getPresentQueue() const {
    return context ? context->getPresentQueue() : VK_NULL_HANDLE;
}

uint32_t QueueManager::getGraphicsQueueFamily() const {
    return context ? context->getGraphicsQueueFamily() : 0;
}

uint32_t QueueManager::getComputeQueueFamily() const {
    return context ? context->getComputeQueueFamily() : 0;
}

uint32_t QueueManager::getTransferQueueFamily() const {
    return context ? context->getTransferQueueFamily() : 0;
}

uint32_t QueueManager::getPresentQueueFamily() const {
    return context ? context->getPresentQueueFamily() : 0;
}

bool QueueManager::hasDedicatedComputeQueue() const {
    return context ? context->hasDedicatedComputeQueue() : false;
}

bool QueueManager::hasDedicatedTransferQueue() const {
    return context ? context->hasDedicatedTransferQueue() : false;
}

bool QueueManager::supportsAsyncCompute() const {
    // True async compute requires dedicated compute queue
    return hasDedicatedComputeQueue();
}

VkCommandPool QueueManager::getCommandPool(CommandPoolType type) const {
    switch (type) {
        case CommandPoolType::Graphics:
            return graphicsCommandPool.get();
        case CommandPoolType::Compute:
            return computeCommandPool.get();
        case CommandPoolType::Transfer:
            return transferCommandPool.get();
        default:
            return VK_NULL_HANDLE;
    }
}

VkCommandBuffer QueueManager::getGraphicsCommandBuffer(uint32_t frameIndex) const {
    if (frameIndex >= graphicsCommandBuffers.size()) {
        std::cerr << "QueueManager: Graphics command buffer index out of range: " << frameIndex << std::endl;
        return VK_NULL_HANDLE;
    }
    return graphicsCommandBuffers[frameIndex];
}

VkCommandBuffer QueueManager::getComputeCommandBuffer(uint32_t frameIndex) const {
    if (frameIndex >= computeCommandBuffers.size()) {
        std::cerr << "QueueManager: Compute command buffer index out of range: " << frameIndex << std::endl;
        return VK_NULL_HANDLE;
    }
    return computeCommandBuffers[frameIndex];
}

QueueManager::TransferCommand QueueManager::allocateTransferCommand() {
    if (!context || !transferCommandPool) {
        std::cerr << "QueueManager: Cannot allocate transfer command - not initialized" << std::endl;
        return {};
    }
    
    TransferCommand command;
    command.sourcePool = transferCommandPool.get();
    
    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool.get();
    allocInfo.commandBufferCount = 1;
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    if (vk.vkAllocateCommandBuffers(device, &allocInfo, &command.commandBuffer) != VK_SUCCESS) {
        std::cerr << "QueueManager: Failed to allocate transfer command buffer" << std::endl;
        return {};
    }
    
    // Create fence for completion tracking
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    command.fence = vulkan_raii::create_fence(context, &fenceInfo);
    if (!command.fence) {
        std::cerr << "QueueManager: Failed to create transfer fence" << std::endl;
        vk.vkFreeCommandBuffers(device, transferCommandPool.get(), 1, &command.commandBuffer);
        return {};
    }
    
    telemetry.recordTransferAllocation();
    return command;
}

void QueueManager::freeTransferCommand(TransferCommand& command) {
    if (!context || !command.isValid()) {
        return;
    }
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    // Free command buffer
    if (command.commandBuffer != VK_NULL_HANDLE && command.sourcePool != VK_NULL_HANDLE) {
        vk.vkFreeCommandBuffers(device, command.sourcePool, 1, &command.commandBuffer);
        command.commandBuffer = VK_NULL_HANDLE;
    }
    
    // RAII fence will clean up automatically
    command.fence.reset();
    command.sourcePool = VK_NULL_HANDLE;
    
    telemetry.recordTransferDeallocation();
}

bool QueueManager::isTransferComplete(const TransferCommand& command) const {
    if (!context || !command.fence) {
        return true; // Invalid commands are considered complete
    }
    
    VkResult result = context->getLoader().vkGetFenceStatus(context->getDevice(), command.fence.get());
    return result == VK_SUCCESS;
}

void QueueManager::waitForTransfer(const TransferCommand& command) const {
    if (!context || !command.fence) {
        return;
    }
    
    VkFence fenceHandle = command.fence.get();
    context->getLoader().vkWaitForFences(context->getDevice(), 1, &fenceHandle, VK_TRUE, UINT64_MAX);
}

void QueueManager::resetCommandBuffersForFrame(uint32_t frameIndex) {
    if (!context) return;
    
    const auto& vk = context->getLoader();
    
    // Reset graphics command buffer for this frame
    if (frameIndex < graphicsCommandBuffers.size()) {
        vk.vkResetCommandBuffer(graphicsCommandBuffers[frameIndex], 0);
    }
    
    // Reset compute command buffer for this frame
    if (frameIndex < computeCommandBuffers.size()) {
        vk.vkResetCommandBuffer(computeCommandBuffers[frameIndex], 0);
    }
}

void QueueManager::resetAllCommandBuffers() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        resetCommandBuffersForFrame(i);
    }
}

void QueueManager::logTelemetry() const {
    std::cout << "QueueManager Telemetry:" << std::endl;
    std::cout << "  Graphics submissions: " << telemetry.graphicsSubmissions << std::endl;
    std::cout << "  Compute submissions: " << telemetry.computeSubmissions << std::endl;
    std::cout << "  Transfer submissions: " << telemetry.transferSubmissions << std::endl;
    std::cout << "  Present submissions: " << telemetry.presentSubmissions << std::endl;
    std::cout << "  Active transfer commands: " << telemetry.activeTransferCommands << std::endl;
    std::cout << "  Peak transfer commands: " << telemetry.peakTransferCommands << std::endl;
    std::cout << "  Total transfer allocations: " << telemetry.totalTransferAllocations << std::endl;
}

bool QueueManager::createCommandPools() {
    if (!context) return false;
    
    // Create graphics command pool
    VkCommandPoolCreateInfo graphicsPoolInfo{};
    graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphicsPoolInfo.flags = getCommandPoolFlags(CommandPoolType::Graphics);
    graphicsPoolInfo.queueFamilyIndex = getQueueFamilyForPool(CommandPoolType::Graphics);
    
    graphicsCommandPool = vulkan_raii::create_command_pool(context, &graphicsPoolInfo);
    if (!graphicsCommandPool) {
        std::cerr << "QueueManager: Failed to create graphics command pool" << std::endl;
        return false;
    }
    
    // Create compute command pool
    VkCommandPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    computePoolInfo.flags = getCommandPoolFlags(CommandPoolType::Compute);
    computePoolInfo.queueFamilyIndex = getQueueFamilyForPool(CommandPoolType::Compute);
    
    computeCommandPool = vulkan_raii::create_command_pool(context, &computePoolInfo);
    if (!computeCommandPool) {
        std::cerr << "QueueManager: Failed to create compute command pool" << std::endl;
        return false;
    }
    
    // Create transfer command pool
    VkCommandPoolCreateInfo transferPoolInfo{};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.flags = getCommandPoolFlags(CommandPoolType::Transfer);
    transferPoolInfo.queueFamilyIndex = getQueueFamilyForPool(CommandPoolType::Transfer);
    
    transferCommandPool = vulkan_raii::create_command_pool(context, &transferPoolInfo);
    if (!transferCommandPool) {
        std::cerr << "QueueManager: Failed to create transfer command pool" << std::endl;
        return false;
    }
    
    return true;
}

bool QueueManager::createFrameCommandBuffers() {
    if (!context || !graphicsCommandPool || !computeCommandPool) {
        return false;
    }
    
    // Allocate graphics command buffers
    graphicsCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo graphicsAllocInfo{};
    graphicsAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    graphicsAllocInfo.commandPool = graphicsCommandPool.get();
    graphicsAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    graphicsAllocInfo.commandBufferCount = static_cast<uint32_t>(graphicsCommandBuffers.size());
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    if (vk.vkAllocateCommandBuffers(device, &graphicsAllocInfo, graphicsCommandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "QueueManager: Failed to allocate graphics command buffers" << std::endl;
        return false;
    }
    
    // Allocate compute command buffers
    computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo computeAllocInfo{};
    computeAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    computeAllocInfo.commandPool = computeCommandPool.get();
    computeAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    computeAllocInfo.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers.size());
    
    if (vk.vkAllocateCommandBuffers(device, &computeAllocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "QueueManager: Failed to allocate compute command buffers" << std::endl;
        return false;
    }
    
    return true;
}

VkCommandPoolCreateFlags QueueManager::getCommandPoolFlags(CommandPoolType type) const {
    switch (type) {
        case CommandPoolType::Graphics:
            // Graphics: Persistent command buffers that can be reset individually
            return VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            
        case CommandPoolType::Compute:
            // Compute: Short-lived dispatches, optimize for frequent allocation/deallocation
            return VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            
        case CommandPoolType::Transfer:
            // Transfer: One-time use command buffers
            return VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            
        default:
            return 0;
    }
}

uint32_t QueueManager::getQueueFamilyForPool(CommandPoolType type) const {
    if (!context) return 0;
    
    switch (type) {
        case CommandPoolType::Graphics:
            return context->getGraphicsQueueFamily();
        case CommandPoolType::Compute:
            return context->getComputeQueueFamily();
        case CommandPoolType::Transfer:
            return context->getTransferQueueFamily();
        default:
            return context->getGraphicsQueueFamily();
    }
}