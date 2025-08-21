#include "resource_context.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_constants.h"

// Include all the new modular managers
#include "../core/memory_allocator.h"
#include "../buffers/buffer_factory.h"
#include "descriptor_pool_manager.h"
#include "graphics_resource_manager.h"
#include "../buffers/buffer_manager.h"

// Define the forward declared types for proper compilation
namespace {
    using DescriptorPoolConfig = DescriptorPoolManager::DescriptorPoolConfig;
    using MemoryStats = MemoryAllocator::MemoryStats;
}

#include <iostream>

ResourceContext::ResourceContext() {
}

ResourceContext::~ResourceContext() {
    cleanup();
}

bool ResourceContext::initialize(const VulkanContext& context, QueueManager* queueManager) {
    this->context = &context;
    
    // Initialize command executor first
    if (queueManager) {
        if (!executor.initialize(context, queueManager)) {
            std::cerr << "Failed to initialize command executor!" << std::endl;
            return false;
        }
    }
    
    // Initialize all specialized managers
    if (!initializeManagers(queueManager)) {
        std::cerr << "Failed to initialize resource managers!" << std::endl;
        return false;
    }
    
    // Setup dependencies between managers
    setupManagerDependencies();
    
    return true;
}

void ResourceContext::cleanup() {
    cleanupBeforeContextDestruction();
    
    // Run cleanup callbacks in reverse order
    for (auto it = cleanupCallbacks.rbegin(); it != cleanupCallbacks.rend(); ++it) {
        (*it)();
    }
    cleanupCallbacks.clear();
    
    // Cleanup in reverse order of initialization
    cleanupManagers();
    executor.cleanup();
    
    context = nullptr;
}

void ResourceContext::cleanupBeforeContextDestruction() {
    // Cleanup RAII resources in reverse order of initialization
    if (graphicsResourceManager) {
        graphicsResourceManager->cleanupBeforeContextDestruction();
    }
    
    if (bufferManager) {
        // No RAII cleanup needed in BufferManager currently
    }
    
    
    if (descriptorPoolManager) {
        // No RAII cleanup needed in DescriptorPoolManager currently
    }
    
    if (bufferFactory) {
        bufferFactory->cleanupBeforeContextDestruction();
    }
    
    if (memoryAllocator) {
        // No RAII cleanup needed in MemoryAllocator currently
    }
    
    // Cleanup command executor RAII resources
    executor.cleanupBeforeContextDestruction();
}

// Core resource creation (delegated to BufferFactory)
ResourceHandle ResourceContext::createBuffer(VkDeviceSize size, 
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties) {
    return bufferFactory ? bufferFactory->createBuffer(size, usage, properties) : ResourceHandle{};
}

ResourceHandle ResourceContext::createMappedBuffer(VkDeviceSize size,
                                                  VkBufferUsageFlags usage,
                                                  VkMemoryPropertyFlags properties) {
    return bufferFactory ? bufferFactory->createMappedBuffer(size, usage, properties) : ResourceHandle{};
}

ResourceHandle ResourceContext::createImage(uint32_t width, uint32_t height,
                                           VkFormat format,
                                           VkImageUsageFlags usage,
                                           VkMemoryPropertyFlags properties,
                                           VkSampleCountFlagBits samples) {
    return bufferFactory ? bufferFactory->createImage(width, height, format, usage, properties, samples) : ResourceHandle{};
}

ResourceHandle ResourceContext::createImageView(const ResourceHandle& imageHandle,
                                               VkFormat format,
                                               VkImageAspectFlags aspectFlags) {
    return bufferFactory ? bufferFactory->createImageView(imageHandle, format, aspectFlags) : ResourceHandle{};
}

void ResourceContext::destroyResource(ResourceHandle& handle) {
    if (bufferFactory) {
        bufferFactory->destroyResource(handle);
    }
}

// Transfer operations (delegated to BufferManager)
bool ResourceContext::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return bufferManager ? bufferManager->copyToBuffer(dst, data, size, offset) : false;
}

bool ResourceContext::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    return bufferManager ? bufferManager->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset) : false;
}

CommandExecutor::AsyncTransfer ResourceContext::copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return bufferManager ? bufferManager->copyToBufferAsync(dst, data, size, offset) : CommandExecutor::AsyncTransfer{};
}

// Descriptor management (delegated to DescriptorPoolManager)
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

vulkan_raii::DescriptorPool ResourceContext::createDescriptorPool() {
    return descriptorPoolManager ? descriptorPoolManager->createDescriptorPool() : vulkan_raii::DescriptorPool{};
}

void ResourceContext::destroyDescriptorPool(VkDescriptorPool pool) {
    if (descriptorPoolManager) {
        descriptorPoolManager->destroyDescriptorPool(pool);
    }
}

// Graphics resources (delegated to GraphicsResourceManager)
bool ResourceContext::createGraphicsResources() {
    return graphicsResourceManager ? graphicsResourceManager->createAllGraphicsResources() : false;
}

bool ResourceContext::recreateGraphicsResources() {
    return graphicsResourceManager ? graphicsResourceManager->recreateGraphicsResources() : false;
}

bool ResourceContext::updateGraphicsDescriptors(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    return graphicsResourceManager ? graphicsResourceManager->updateDescriptorSetsWithEntityAndPositionBuffers(entityBuffer, positionBuffer) : false;
}

// Individual graphics resource creation (for legacy compatibility)
bool ResourceContext::createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    return graphicsResourceManager ? graphicsResourceManager->createGraphicsDescriptorPool(descriptorSetLayout) : false;
}

bool ResourceContext::createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    // This is typically handled by createDescriptorResources, but we can delegate to the graphics manager
    if (!graphicsResourceManager) return false;
    return graphicsResourceManager->createGraphicsDescriptorSets(descriptorSetLayout);
}

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

// Legacy compatibility - staging buffer direct access
StagingRingBuffer& ResourceContext::getStagingBuffer() {
    return getBufferManager()->getPrimaryStagingBuffer();
}

const StagingRingBuffer& ResourceContext::getStagingBuffer() const {
    return getBufferManager()->getPrimaryStagingBuffer();
}

// Statistics and monitoring (delegated to MemoryAllocator)
VkDeviceSize ResourceContext::getTotalAllocatedMemory() const {
    if (!memoryAllocator) return 0;
    auto stats = memoryAllocator->getMemoryStats();
    return stats.totalAllocated;
}

VkDeviceSize ResourceContext::getAvailableMemory() const {
    if (!memoryAllocator) return 0;
    auto stats = memoryAllocator->getMemoryStats();
    // Calculate available as peak minus current usage (approximation)
    return stats.peakUsage > stats.totalAllocated ? stats.peakUsage - stats.totalAllocated : 0;
}

uint32_t ResourceContext::getAllocationCount() const {
    if (!memoryAllocator) return 0;
    auto stats = memoryAllocator->getMemoryStats();
    return stats.activeAllocations;
}

// Legacy memory stats access (for backward compatibility)
ResourceContext::SimpleMemoryStats ResourceContext::getMemoryStats() const {
    SimpleMemoryStats simpleStats;
    
    if (memoryAllocator) {
        auto stats = memoryAllocator->getMemoryStats();
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

bool ResourceContext::isUnderMemoryPressure() const {
    return memoryAllocator ? memoryAllocator->isUnderMemoryPressure() : false;
}

bool ResourceContext::attemptMemoryRecovery() {
    return memoryAllocator ? memoryAllocator->attemptMemoryRecovery() : false;
}

bool ResourceContext::optimizeResources() {
    bool success = true;
    
    // Optimize each manager
    if (memoryAllocator) {
        success &= memoryAllocator->attemptMemoryRecovery();
    }
    
    if (bufferManager) {
        success &= bufferManager->tryOptimizeMemory();
    }
    
    if (graphicsResourceManager) {
        success &= graphicsResourceManager->optimizeGraphicsMemoryUsage();
    }
    
    return success;
}

// Private implementation methods
bool ResourceContext::initializeManagers(QueueManager* queueManager) {
    // Initialize in dependency order
    
    // 1. Memory allocator (foundation)
    memoryAllocator = std::make_unique<MemoryAllocator>();
    if (!memoryAllocator->initialize(*context)) {
        std::cerr << "Failed to initialize memory allocator!" << std::endl;
        return false;
    }
    
    // 2. Buffer factory (depends on memory allocator)
    bufferFactory = std::make_unique<BufferFactory>();
    if (!bufferFactory->initialize(*context, memoryAllocator.get())) {
        std::cerr << "Failed to initialize buffer factory!" << std::endl;
        return false;
    }
    
    // 3. Descriptor pool manager
    descriptorPoolManager = std::make_unique<DescriptorPoolManager>();
    if (!descriptorPoolManager->initialize(*context)) {
        std::cerr << "Failed to initialize descriptor pool manager!" << std::endl;
        return false;
    }
    
    // 4. Graphics resource manager (depends on buffer factory)
    graphicsResourceManager = std::make_unique<GraphicsResourceManager>();
    if (!graphicsResourceManager->initialize(*context, bufferFactory.get())) {
        std::cerr << "Failed to initialize graphics resource manager!" << std::endl;
        return false;
    }
    
    // 5. Unified buffer manager (replaces staging, gpu buffer, and transfer managers)
    bufferManager = std::make_unique<BufferManager>();
    if (!bufferManager->initialize(this, bufferFactory.get(), &executor, STAGING_BUFFER_SIZE)) {
        std::cerr << "Failed to initialize buffer manager!" << std::endl;
        return false;
    }
    
    
    return true;
}

void ResourceContext::setupManagerDependencies() {
    // Setup cross-manager dependencies
    if (bufferFactory && bufferManager) {
        bufferFactory->setStagingBuffer(&bufferManager->getPrimaryStagingBuffer());
        bufferFactory->setCommandExecutor(&executor);
    }
}

void ResourceContext::cleanupManagers() {
    // Cleanup in reverse order of initialization
    bufferManager.reset();
    graphicsResourceManager.reset();
    descriptorPoolManager.reset();
    bufferFactory.reset();
    memoryAllocator.reset();
}