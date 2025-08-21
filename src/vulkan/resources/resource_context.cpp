#include "resource_context.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_constants.h"

// Include all the new modular managers
#include "memory_allocator.h"
#include "buffer_factory.h"
#include "descriptor_pool_manager.h"
#include "graphics_resource_manager.h"
#include "staging_buffer_manager.h"
#include "gpu_buffer_manager.h"
#include "transfer_manager.h"
#include "graphics_resource_facade.h"

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
    // Delegate to graphics facade for proper cleanup ordering
    if (graphicsFacade) {
        graphicsFacade->cleanupGraphicsResources();
    }
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

// Transfer operations (delegated to TransferManager)
bool ResourceContext::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return transferManager ? transferManager->copyToBuffer(dst, data, size, offset) : false;
}

bool ResourceContext::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    return transferManager ? transferManager->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset) : false;
}

CommandExecutor::AsyncTransfer ResourceContext::copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return transferManager ? transferManager->copyToBufferAsync(dst, data, size, offset) : CommandExecutor::AsyncTransfer{};
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

// Graphics resources (delegated to GraphicsResourceFacade)
bool ResourceContext::createGraphicsResources() {
    return graphicsFacade ? graphicsFacade->createAllGraphicsResources() : false;
}

bool ResourceContext::recreateGraphicsResources() {
    return graphicsFacade ? graphicsFacade->recreateGraphicsResources() : false;
}

bool ResourceContext::updateGraphicsDescriptors(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    return graphicsFacade ? graphicsFacade->updateDescriptorSetsForEntityRendering(entityBuffer, positionBuffer) : false;
}

// Individual graphics resource creation (for legacy compatibility)
bool ResourceContext::createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    return graphicsFacade ? graphicsFacade->createDescriptorResources(descriptorSetLayout) : false;
}

bool ResourceContext::createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    // This is typically handled by createDescriptorResources, but we can delegate to the graphics manager
    if (!graphicsResourceManager) return false;
    return graphicsResourceManager->createGraphicsDescriptorSets(descriptorSetLayout);
}

const std::vector<VkBuffer>& ResourceContext::getUniformBuffers() const {
    static const std::vector<VkBuffer> empty;
    return graphicsFacade ? graphicsFacade->getUniformBuffers() : empty;
}

const std::vector<void*>& ResourceContext::getUniformBuffersMapped() const {
    static const std::vector<void*> empty;
    return graphicsFacade ? graphicsFacade->getUniformBuffersMapped() : empty;
}

VkBuffer ResourceContext::getVertexBuffer() const {
    return graphicsFacade ? graphicsFacade->getVertexBuffer() : VK_NULL_HANDLE;
}

VkBuffer ResourceContext::getIndexBuffer() const {
    return graphicsFacade ? graphicsFacade->getIndexBuffer() : VK_NULL_HANDLE;
}

uint32_t ResourceContext::getIndexCount() const {
    return graphicsFacade ? graphicsFacade->getIndexCount() : 0;
}

VkDescriptorPool ResourceContext::getGraphicsDescriptorPool() const {
    return graphicsFacade ? graphicsFacade->getDescriptorPool() : VK_NULL_HANDLE;
}

const std::vector<VkDescriptorSet>& ResourceContext::getGraphicsDescriptorSets() const {
    static const std::vector<VkDescriptorSet> empty;
    return graphicsFacade ? graphicsFacade->getDescriptorSets() : empty;
}

// Legacy compatibility - staging buffer direct access
StagingRingBuffer& ResourceContext::getStagingBuffer() {
    return getStagingManager()->getPrimaryBuffer();
}

const StagingRingBuffer& ResourceContext::getStagingBuffer() const {
    return getStagingManager()->getPrimaryBuffer();
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
    
    if (stagingManager) {
        success &= stagingManager->tryDefragment();
    }
    
    if (transferManager) {
        success &= transferManager->tryOptimizeTransfers();
    }
    
    if (graphicsFacade) {
        success &= graphicsFacade->optimizeGraphicsMemoryUsage();
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
    
    // 5. Staging buffer manager
    stagingManager = std::make_unique<StagingBufferManager>();
    if (!stagingManager->initialize(*context, STAGING_BUFFER_SIZE)) {
        std::cerr << "Failed to initialize staging buffer manager!" << std::endl;
        return false;
    }
    
    // 6. GPU buffer manager (depends on staging manager)
    gpuBufferManager = std::make_unique<GPUBufferManager>();
    if (!gpuBufferManager->initialize(this, stagingManager.get())) {
        std::cerr << "Failed to initialize GPU buffer manager!" << std::endl;
        return false;
    }
    
    // 7. Transfer manager (depends on multiple managers)
    transferManager = std::make_unique<TransferManager>();
    if (!transferManager->initialize(this, bufferFactory.get(), stagingManager.get(), &executor)) {
        std::cerr << "Failed to initialize transfer manager!" << std::endl;
        return false;
    }
    
    // 8. Graphics facade (high-level coordination)
    graphicsFacade = std::make_unique<GraphicsResourceFacade>();
    if (!graphicsFacade->initialize(this, graphicsResourceManager.get())) {
        std::cerr << "Failed to initialize graphics resource facade!" << std::endl;
        return false;
    }
    
    return true;
}

void ResourceContext::setupManagerDependencies() {
    // Setup cross-manager dependencies
    if (bufferFactory && stagingManager && transferManager) {
        bufferFactory->setStagingBuffer(&stagingManager->getPrimaryBuffer());
        bufferFactory->setCommandExecutor(&executor);
    }
}

void ResourceContext::cleanupManagers() {
    // Cleanup in reverse order of initialization
    graphicsFacade.reset();
    transferManager.reset();
    gpuBufferManager.reset();
    stagingManager.reset();
    graphicsResourceManager.reset();
    descriptorPoolManager.reset();
    bufferFactory.reset();
    memoryAllocator.reset();
}