#include "resource_coordinator.h"
#include "resource_factory.h"
#include "transfer_manager.h"
#include "memory_allocator.h"
#include "validation_utils.h"
#include "../managers/descriptor_pool_manager.h"
#include "../managers/graphics_resource_manager.h"
#include "../buffers/buffer_manager.h"
#include "../buffers/staging_buffer_pool.h"
#include "../buffers/buffer_registry.h"
#include "../buffers/transfer_orchestrator.h"
#include "../buffers/buffer_factory.h"
#include "../../core/vulkan_context.h"
#include "../../core/queue_manager.h"
#include <stdexcept>

ResourceCoordinator::ResourceCoordinator() {
}

ResourceCoordinator::~ResourceCoordinator() {
    cleanup();
}

bool ResourceCoordinator::initialize(const VulkanContext& context, QueueManager* queueManager) {
    if (!ValidationUtils::validateDependencies("ResourceCoordinator::initialize", &context, queueManager)) {
        return false;
    }
    
    this->context = &context;
    
    // Initialize command executor first
    if (!executor.initialize(context, queueManager)) {
        ValidationUtils::logInitializationError("ResourceCoordinator", "CommandExecutor initialization failed");
        return false;
    }
    
    if (!initializeManagers(queueManager)) {
        ValidationUtils::logInitializationError("ResourceCoordinator", "Manager initialization failed");
        cleanup();
        return false;
    }
    
    setupManagerDependencies();
    return true;
}

void ResourceCoordinator::cleanup() {
    cleanupManagers();
    executor.cleanup();
    context = nullptr;
}

void ResourceCoordinator::cleanupBeforeContextDestruction() {
    if (memoryAllocator) {
        // TODO: Add cleanupBeforeContextDestruction to MemoryAllocator if needed
    }
    if (resourceFactory) {
        // TODO: Add cleanupBeforeContextDestruction to ResourceFactory if needed
    }
    // TODO: Fix BufferManager integration
    // if (bufferManager) {
    //     bufferManager->cleanup(); // Use cleanup instead
    // }
}

ResourceHandle ResourceCoordinator::createBuffer(VkDeviceSize size, 
                                                VkBufferUsageFlags usage,
                                                VkMemoryPropertyFlags properties) {
    if (!resourceFactory) {
        ValidationUtils::logError("ResourceCoordinator", "createBuffer", "ResourceFactory not initialized");
        return {};
    }
    
    return resourceFactory->createBuffer(size, usage, properties);
}

ResourceHandle ResourceCoordinator::createMappedBuffer(VkDeviceSize size,
                                                      VkBufferUsageFlags usage,
                                                      VkMemoryPropertyFlags properties) {
    if (!resourceFactory) {
        ValidationUtils::logError("ResourceCoordinator", "createMappedBuffer", "ResourceFactory not initialized");
        return {};
    }
    
    return resourceFactory->createMappedBuffer(size, usage, properties);
}

ResourceHandle ResourceCoordinator::createImage(uint32_t width, uint32_t height,
                                               VkFormat format,
                                               VkImageUsageFlags usage,
                                               VkMemoryPropertyFlags properties,
                                               VkSampleCountFlagBits samples) {
    if (!resourceFactory) {
        ValidationUtils::logError("ResourceCoordinator", "createImage", "ResourceFactory not initialized");
        return {};
    }
    
    return resourceFactory->createImage(width, height, format, usage, properties, samples);
}

ResourceHandle ResourceCoordinator::createImageView(const ResourceHandle& imageHandle,
                                                   VkFormat format,
                                                   VkImageAspectFlags aspectFlags) {
    if (!resourceFactory) {
        ValidationUtils::logError("ResourceCoordinator", "createImageView", "ResourceFactory not initialized");
        return {};
    }
    
    return resourceFactory->createImageView(imageHandle, format, aspectFlags);
}

void ResourceCoordinator::destroyResource(ResourceHandle& handle) {
    if (resourceFactory) {
        resourceFactory->destroyResource(handle);
    }
}

bool ResourceCoordinator::copyToBuffer(const ResourceHandle& dst, const void* data, 
                                      VkDeviceSize size, VkDeviceSize offset) {
    if (!transferManager) {
        ValidationUtils::logError("ResourceCoordinator", "copyToBuffer", "TransferManager not initialized");
        return false;
    }
    
    return transferManager->copyToBuffer(dst, data, size, offset);
}

bool ResourceCoordinator::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst,
                                            VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!transferManager) {
        ValidationUtils::logError("ResourceCoordinator", "copyBufferToBuffer", "TransferManager not initialized");
        return false;
    }
    
    return transferManager->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset);
}

CommandExecutor::AsyncTransfer ResourceCoordinator::copyToBufferAsync(const ResourceHandle& dst, const void* data,
                                                                     VkDeviceSize size, VkDeviceSize offset) {
    if (!transferManager) {
        ValidationUtils::logError("ResourceCoordinator", "copyToBufferAsync", "TransferManager not initialized");
        return {};
    }
    
    return transferManager->copyToBufferAsync(dst, data, size, offset);
}

// Manager access methods
MemoryAllocator* ResourceCoordinator::getMemoryAllocator() const {
    return memoryAllocator.get();
}

ResourceFactory* ResourceCoordinator::getResourceFactory() const {
    return resourceFactory.get();
}

TransferManager* ResourceCoordinator::getTransferManager() const {
    return transferManager.get();
}

DescriptorPoolManager* ResourceCoordinator::getDescriptorPoolManager() const {
    return descriptorPoolManager.get();
}

GraphicsResourceManager* ResourceCoordinator::getGraphicsManager() const {
    return graphicsResourceManager.get();
}

BufferManager* ResourceCoordinator::getBufferManager() const {
    return bufferManager.get();
}

StagingBufferPool& ResourceCoordinator::getStagingBuffer() {
    // TODO: Fix BufferManager integration
    throw std::runtime_error("BufferManager integration needs to be fixed");
}

const StagingBufferPool& ResourceCoordinator::getStagingBuffer() const {
    // TODO: Fix BufferManager integration
    throw std::runtime_error("BufferManager integration needs to be fixed");
}

bool ResourceCoordinator::isUnderMemoryPressure() const {
    return memoryAllocator ? memoryAllocator->isUnderMemoryPressure() : false;
}

bool ResourceCoordinator::attemptMemoryRecovery() {
    return memoryAllocator ? memoryAllocator->attemptMemoryRecovery() : false;
}

VkDeviceSize ResourceCoordinator::getTotalAllocatedMemory() const {
    if (!memoryAllocator) return 0;
    return memoryAllocator->getMemoryStats().totalAllocated;
}

VkDeviceSize ResourceCoordinator::getAvailableMemory() const {
    if (!memoryAllocator) return 0;
    auto stats = memoryAllocator->getMemoryStats();
    return stats.totalAllocated > stats.totalFreed ? stats.totalAllocated - stats.totalFreed : 0;
}

uint32_t ResourceCoordinator::getAllocationCount() const {
    if (!memoryAllocator) return 0;
    return memoryAllocator->getMemoryStats().activeAllocations;
}

bool ResourceCoordinator::optimizeResources() {
    bool success = true;
    
    if (memoryAllocator) {
        success &= memoryAllocator->attemptMemoryRecovery();
    }
    
    // TODO: Fix BufferManager integration  
    // if (bufferManager) {
    //     success &= bufferManager->tryOptimizeMemory();
    // }
    
    return success;
}

bool ResourceCoordinator::initializeManagers(QueueManager* queueManager) {
    // Initialize in dependency order
    
    // 1. MemoryAllocator (no dependencies)
    memoryAllocator = std::make_unique<MemoryAllocator>();
    if (!memoryAllocator->initialize(*context)) {
        return false;
    }
    
    // 2. ResourceFactory (depends on MemoryAllocator)
    resourceFactory = std::make_unique<ResourceFactory>();
    if (!resourceFactory->initialize(*context, memoryAllocator.get())) {
        return false;
    }
    
    // 3. BufferManager (complex initialization with multiple dependencies)
    bufferManager = std::make_unique<BufferManager>();
    // TODO: BufferManager needs ResourceContext, but we're trying to eliminate it
    // For now, skip BufferManager initialization to fix compilation
    // This will need proper refactoring later
    // if (!bufferManager->initialize(*context, queueManager)) {
    //     return false;
    // }
    
    // 4. TransferManager (depends on TransferOrchestrator from BufferManager)
    transferManager = std::make_unique<TransferManager>();
    // TODO: Initialize with BufferManager's TransferOrchestrator
    
    // 5. DescriptorPoolManager (minimal dependencies)
    descriptorPoolManager = std::make_unique<DescriptorPoolManager>();
    if (!descriptorPoolManager->initialize(*context)) {
        return false;
    }
    
    // 6. GraphicsResourceManager (depends on multiple managers)
    graphicsResourceManager = std::make_unique<GraphicsResourceManager>();
    if (!graphicsResourceManager->initialize(*context, resourceFactory->getBufferFactory())) {
        return false;
    }
    
    return true;
}

void ResourceCoordinator::setupManagerDependencies() {
    // Set up cross-manager dependencies
    if (resourceFactory && resourceFactory->getBufferFactory()) {
        auto bufferFactory = resourceFactory->getBufferFactory();
        bufferFactory->setCommandExecutor(&executor);
        
        // TODO: Fix BufferManager integration
        // if (bufferManager) {
        //     bufferFactory->setStagingBuffer(&bufferManager->getStagingPool());
        // }
    }
}

void ResourceCoordinator::cleanupManagers() {
    // Cleanup in reverse order of initialization
    graphicsResourceManager.reset();
    descriptorPoolManager.reset();
    transferManager.reset();
    bufferManager.reset();
    resourceFactory.reset();
    memoryAllocator.reset();
}