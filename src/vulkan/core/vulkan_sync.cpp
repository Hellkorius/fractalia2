#include "vulkan_sync.h"
#include "vulkan_function_loader.h"
#include <iostream>

VulkanSync::VulkanSync() {
}

VulkanSync::~VulkanSync() {
    cleanup();
}

bool VulkanSync::initialize(const VulkanContext& context) {
    this->context = &context;
    
    if (context.getDevice() == VK_NULL_HANDLE) {
        std::cerr << "VulkanSync: Invalid VulkanContext provided!" << std::endl;
        return false;
    }
    
    if (!createSyncObjects()) {
        std::cerr << "VulkanSync: Failed to create synchronization objects!" << std::endl;
        return false;
    }
    
    std::cout << "VulkanSync: Initialized synchronization objects for " << MAX_FRAMES_IN_FLIGHT << " frames" << std::endl;
    return true;
}

void VulkanSync::cleanup() {
    cleanupBeforeContextDestruction();
    context = nullptr;
}

void VulkanSync::cleanupBeforeContextDestruction() {
    // Clear RAII wrappers before context destruction
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    computeFinishedSemaphores.clear();
    inFlightFences.clear();
    computeFences.clear();
}

VkSemaphore VulkanSync::getImageAvailableSemaphore(size_t index) const {
    if (index >= imageAvailableSemaphores.size()) {
        std::cerr << "VulkanSync: Image available semaphore index out of range: " << index << std::endl;
        return VK_NULL_HANDLE;
    }
    return imageAvailableSemaphores[index].get();
}

VkSemaphore VulkanSync::getRenderFinishedSemaphore(size_t index) const {
    if (index >= renderFinishedSemaphores.size()) {
        std::cerr << "VulkanSync: Render finished semaphore index out of range: " << index << std::endl;
        return VK_NULL_HANDLE;
    }
    return renderFinishedSemaphores[index].get();
}

VkSemaphore VulkanSync::getComputeFinishedSemaphore(size_t index) const {
    if (index >= computeFinishedSemaphores.size()) {
        std::cerr << "VulkanSync: Compute finished semaphore index out of range: " << index << std::endl;
        return VK_NULL_HANDLE;
    }
    return computeFinishedSemaphores[index].get();
}

VkFence VulkanSync::getInFlightFence(size_t index) const {
    if (index >= inFlightFences.size()) {
        std::cerr << "VulkanSync: In-flight fence index out of range: " << index << std::endl;
        return VK_NULL_HANDLE;
    }
    return inFlightFences[index].get();
}

VkFence VulkanSync::getComputeFence(size_t index) const {
    if (index >= computeFences.size()) {
        std::cerr << "VulkanSync: Compute fence index out of range: " << index << std::endl;
        return VK_NULL_HANDLE;
    }
    return computeFences[index].get();
}

std::vector<VkSemaphore> VulkanSync::getImageAvailableSemaphores() const {
    std::vector<VkSemaphore> semaphores;
    semaphores.reserve(imageAvailableSemaphores.size());
    for (const auto& sem : imageAvailableSemaphores) {
        semaphores.push_back(sem.get());
    }
    return semaphores;
}

std::vector<VkSemaphore> VulkanSync::getRenderFinishedSemaphores() const {
    std::vector<VkSemaphore> semaphores;
    semaphores.reserve(renderFinishedSemaphores.size());
    for (const auto& sem : renderFinishedSemaphores) {
        semaphores.push_back(sem.get());
    }
    return semaphores;
}

std::vector<VkSemaphore> VulkanSync::getComputeFinishedSemaphores() const {
    std::vector<VkSemaphore> semaphores;
    semaphores.reserve(computeFinishedSemaphores.size());
    for (const auto& sem : computeFinishedSemaphores) {
        semaphores.push_back(sem.get());
    }
    return semaphores;
}

std::vector<VkFence> VulkanSync::getInFlightFences() const {
    std::vector<VkFence> fences;
    fences.reserve(inFlightFences.size());
    for (const auto& fence : inFlightFences) {
        fences.push_back(fence.get());
    }
    return fences;
}

std::vector<VkFence> VulkanSync::getComputeFences() const {
    std::vector<VkFence> fences;
    fences.reserve(computeFences.size());
    for (const auto& fence : computeFences) {
        fences.push_back(fence.get());
    }
    return fences;
}

bool VulkanSync::createSyncObjects() {
    if (!context) {
        return false;
    }
    
    // Resize all synchronization object vectors
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start in signaled state
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        const auto& vk = context->getLoader();
        const VkDevice device = context->getDevice();
        
        // Create semaphores
        VkSemaphore imageAvailableSem;
        if (vk.vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSem) != VK_SUCCESS) {
            std::cerr << "VulkanSync: Failed to create image available semaphore " << i << std::endl;
            return false;
        }
        imageAvailableSemaphores[i] = vulkan_raii::make_semaphore(imageAvailableSem, context);
        
        VkSemaphore renderFinishedSem;
        if (vk.vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSem) != VK_SUCCESS) {
            std::cerr << "VulkanSync: Failed to create render finished semaphore " << i << std::endl;
            return false;
        }
        renderFinishedSemaphores[i] = vulkan_raii::make_semaphore(renderFinishedSem, context);
        
        VkSemaphore computeFinishedSem;
        if (vk.vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSem) != VK_SUCCESS) {
            std::cerr << "VulkanSync: Failed to create compute finished semaphore " << i << std::endl;
            return false;
        }
        computeFinishedSemaphores[i] = vulkan_raii::make_semaphore(computeFinishedSem, context);
        
        // Create fences
        inFlightFences[i] = vulkan_raii::create_fence(context, &fenceInfo);
        if (!inFlightFences[i]) {
            std::cerr << "VulkanSync: Failed to create in-flight fence " << i << std::endl;
            return false;
        }
        
        computeFences[i] = vulkan_raii::create_fence(context, &fenceInfo);
        if (!computeFences[i]) {
            std::cerr << "VulkanSync: Failed to create compute fence " << i << std::endl;
            return false;
        }
    }
    
    return true;
}