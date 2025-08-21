#include "resource_context.h"
#include "../../core/vulkan_context.h"
#include "../core/resource_coordinator.h"
#include "../core/resource_factory.h"
#include "descriptor_pool_manager.h"
#include "graphics_resource_manager.h"
#include "../core/memory_allocator.h"
#include "../buffers/buffer_factory.h"
#include "../buffers/buffer_manager.h"
#include "../buffers/staging_buffer_pool.h"
#include <iostream>

ResourceContext::ResourceContext() {
}

ResourceContext::~ResourceContext() {
    cleanup();
}

bool ResourceContext::initialize(const VulkanContext& context, QueueManager* queueManager) {
    if (initialized) {
        return true;
    }
    
    // Create and initialize ResourceCoordinator
    coordinator = std::make_unique<ResourceCoordinator>();
    if (!coordinator->initialize(context, queueManager)) {
        std::cerr << "Failed to initialize ResourceCoordinator!" << std::endl;
        return false;
    }
    
    // Initialize specialized managers not handled by ResourceCoordinator
    descriptorPoolManager = std::make_unique<DescriptorPoolManager>();
    if (!descriptorPoolManager->initialize(context)) {
        std::cerr << "Failed to initialize DescriptorPoolManager!" << std::endl;
        return false;
    }
    
    graphicsResourceManager = std::make_unique<GraphicsResourceManager>();
    if (!graphicsResourceManager->initialize(context, coordinator->getResourceFactory()->getBufferFactory())) {
        std::cerr << "Failed to initialize GraphicsResourceManager!" << std::endl;
        return false;
    }
    
    initialized = true;
    return true;
}

void ResourceContext::cleanup() {
    if (!initialized) {
        return;
    }
    
    cleanupBeforeContextDestruction();
    
    // Cleanup in reverse order
    graphicsResourceManager.reset();
    descriptorPoolManager.reset();
    coordinator.reset();
    
    initialized = false;
}

void ResourceContext::cleanupBeforeContextDestruction() {
    if (!initialized) {
        return;
    }
    
    // Cleanup RAII resources in reverse order
    if (graphicsResourceManager) {
        graphicsResourceManager->cleanupBeforeContextDestruction();
    }
    
    if (coordinator) {
        coordinator->cleanupBeforeContextDestruction();
    }
}

// Context access
const VulkanContext* ResourceContext::getContext() const {
    return coordinator ? coordinator->getContext() : nullptr;
}

// Core resource creation (delegates to ResourceCoordinator)
ResourceHandle ResourceContext::createBuffer(VkDeviceSize size, 
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties) {
    return coordinator ? coordinator->createBuffer(size, usage, properties) : ResourceHandle{};
}

ResourceHandle ResourceContext::createMappedBuffer(VkDeviceSize size,
                                                  VkBufferUsageFlags usage,
                                                  VkMemoryPropertyFlags properties) {
    return coordinator ? coordinator->createMappedBuffer(size, usage, properties) : ResourceHandle{};
}

ResourceHandle ResourceContext::createImage(uint32_t width, uint32_t height,
                                           VkFormat format,
                                           VkImageUsageFlags usage,
                                           VkMemoryPropertyFlags properties,
                                           VkSampleCountFlagBits samples) {
    return coordinator ? coordinator->createImage(width, height, format, usage, properties, samples) : ResourceHandle{};
}

ResourceHandle ResourceContext::createImageView(const ResourceHandle& imageHandle,
                                               VkFormat format,
                                               VkImageAspectFlags aspectFlags) {
    return coordinator ? coordinator->createImageView(imageHandle, format, aspectFlags) : ResourceHandle{};
}

void ResourceContext::destroyResource(ResourceHandle& handle) {
    if (coordinator) {
        coordinator->destroyResource(handle);
    }
}

// Transfer operations (delegates to ResourceCoordinator)
bool ResourceContext::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return coordinator ? coordinator->copyToBuffer(dst, data, size, offset) : false;
}

bool ResourceContext::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    return coordinator ? coordinator->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset) : false;
}

CommandExecutor::AsyncTransfer ResourceContext::copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return coordinator ? coordinator->copyToBufferAsync(dst, data, size, offset) : CommandExecutor::AsyncTransfer{};
}

// Descriptor management (delegates to DescriptorPoolManager)
vulkan_raii::DescriptorPool ResourceContext::createDescriptorPool() {
    return descriptorPoolManager ? descriptorPoolManager->createDescriptorPool() : vulkan_raii::DescriptorPool{};
}

vulkan_raii::DescriptorPool ResourceContext::createDescriptorPoolWithConfig(uint32_t maxSets, 
                                                                           uint32_t uniformBufferCount, 
                                                                           uint32_t storageBufferCount) {
    if (!descriptorPoolManager) {
        return vulkan_raii::DescriptorPool{};
    }
    
    DescriptorPoolManager::DescriptorPoolConfig config;
    config.maxSets = maxSets;
    config.uniformBuffers = uniformBufferCount;
    config.storageBuffers = storageBufferCount;
    
    return descriptorPoolManager->createDescriptorPool(config);
}

void ResourceContext::destroyDescriptorPool(VkDescriptorPool pool) {
    if (descriptorPoolManager) {
        descriptorPoolManager->destroyDescriptorPool(pool);
    }
}

// Graphics resources (delegates to GraphicsResourceManager)
bool ResourceContext::createGraphicsResources() {
    return graphicsResourceManager ? graphicsResourceManager->createAllGraphicsResources() : false;
}

bool ResourceContext::recreateGraphicsResources() {
    return graphicsResourceManager ? graphicsResourceManager->recreateGraphicsResources() : false;
}

bool ResourceContext::updateGraphicsDescriptors(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    return graphicsResourceManager ? graphicsResourceManager->updateDescriptorSetsWithEntityAndPositionBuffers(entityBuffer, positionBuffer) : false;
}

// Graphics resource individual creation (for legacy compatibility)
bool ResourceContext::createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    return graphicsResourceManager ? graphicsResourceManager->createGraphicsDescriptorPool(descriptorSetLayout) : false;
}

bool ResourceContext::createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    return graphicsResourceManager ? graphicsResourceManager->createGraphicsDescriptorSets(descriptorSetLayout) : false;
}

// Getters - delegates to appropriate managers
const std::vector<VkBuffer>& ResourceContext::getUniformBuffers() const {
    static const std::vector<VkBuffer> empty;
    return graphicsResourceManager ? graphicsResourceManager->getUniformBuffers() : empty;
}

const std::vector<void*>& ResourceContext::getUniformBuffersMapped() const {
    static const std::vector<void*> empty;
    return graphicsResourceManager ? graphicsResourceManager->getUniformBuffersMapped() : empty;
}

VkBuffer ResourceContext::getVertexBuffer() const {
    return graphicsResourceManager ? graphicsResourceManager->getVertexBuffer() : VK_NULL_HANDLE;
}

VkBuffer ResourceContext::getIndexBuffer() const {
    return graphicsResourceManager ? graphicsResourceManager->getIndexBuffer() : VK_NULL_HANDLE;
}

uint32_t ResourceContext::getIndexCount() const {
    return graphicsResourceManager ? graphicsResourceManager->getIndexCount() : 0;
}

VkDescriptorPool ResourceContext::getGraphicsDescriptorPool() const {
    return graphicsResourceManager ? graphicsResourceManager->getDescriptorPool() : VK_NULL_HANDLE;
}

const std::vector<VkDescriptorSet>& ResourceContext::getGraphicsDescriptorSets() const {
    static const std::vector<VkDescriptorSet> empty;
    return graphicsResourceManager ? graphicsResourceManager->getDescriptorSets() : empty;
}

// Manager access for advanced operations
MemoryAllocator* ResourceContext::getMemoryAllocator() const {
    return coordinator ? coordinator->getMemoryAllocator() : nullptr;
}

BufferFactory* ResourceContext::getBufferFactory() const {
    return coordinator && coordinator->getResourceFactory() ? coordinator->getResourceFactory()->getBufferFactory() : nullptr;
}

CommandExecutor* ResourceContext::getCommandExecutor() const {
    return coordinator ? coordinator->getCommandExecutor() : nullptr;
}

BufferManager* ResourceContext::getBufferManager() const {
    return coordinator ? coordinator->getBufferManager() : nullptr;
}

GraphicsResourceManager* ResourceContext::getGraphicsManager() const {
    return graphicsResourceManager.get();
}

// Legacy compatibility - staging buffer direct access
StagingBufferPool& ResourceContext::getStagingBuffer() {
    // Use temporary workaround until BufferManager integration is fixed
    auto bufferManager = getBufferManager();
    if (!bufferManager) {
        throw std::runtime_error("BufferManager not available - circular dependency needs resolution");
    }
    return bufferManager->getPrimaryStagingBuffer();
}

const StagingBufferPool& ResourceContext::getStagingBuffer() const {
    // Use temporary workaround until BufferManager integration is fixed
    auto bufferManager = getBufferManager();
    if (!bufferManager) {
        throw std::runtime_error("BufferManager not available - circular dependency needs resolution");
    }
    return bufferManager->getPrimaryStagingBuffer();
}

// Statistics and monitoring (delegates to ResourceCoordinator)
bool ResourceContext::isUnderMemoryPressure() const {
    return coordinator ? coordinator->isUnderMemoryPressure() : false;
}

bool ResourceContext::attemptMemoryRecovery() {
    return coordinator ? coordinator->attemptMemoryRecovery() : false;
}

// Memory statistics - simplified interface
VkDeviceSize ResourceContext::getTotalAllocatedMemory() const {
    return coordinator ? coordinator->getTotalAllocatedMemory() : 0;
}

VkDeviceSize ResourceContext::getAvailableMemory() const {
    return coordinator ? coordinator->getAvailableMemory() : 0;
}

uint32_t ResourceContext::getAllocationCount() const {
    return coordinator ? coordinator->getAllocationCount() : 0;
}

// Legacy memory stats structure for backward compatibility
ResourceContext::SimpleMemoryStats ResourceContext::getMemoryStats() const {
    SimpleMemoryStats simpleStats;
    
    if (coordinator && coordinator->getMemoryAllocator()) {
        auto stats = coordinator->getMemoryAllocator()->getMemoryStats();
        simpleStats.totalAllocated = stats.totalAllocated;
        simpleStats.totalFreed = stats.totalFreed;
        simpleStats.activeAllocations = stats.activeAllocations;
        simpleStats.peakUsage = stats.peakUsage;
        simpleStats.failedAllocations = stats.failedAllocations;
        simpleStats.memoryPressure = stats.memoryPressure;
        simpleStats.fragmentationRatio = stats.fragmentationRatio;
    }
    
    return simpleStats;
}

// Performance optimization
bool ResourceContext::optimizeResources() {
    bool success = true;
    
    if (coordinator) {
        success &= coordinator->optimizeResources();
    }
    
    if (graphicsResourceManager) {
        success &= graphicsResourceManager->optimizeGraphicsMemoryUsage();
    }
    
    return success;
}