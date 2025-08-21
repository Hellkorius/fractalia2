#include "buffer_manager.h"
#include "gpu_buffer.h"
#include "../core/resource_context_interface.h"
#include "buffer_factory.h"
#include <iostream>
#include <cstring>

BufferManager::BufferManager() {
    stagingPool = std::make_unique<StagingBufferPool>();
    bufferRegistry = std::make_unique<BufferRegistry>();
    transferOrchestrator = std::make_unique<TransferOrchestrator>();
    statisticsCollector = std::make_unique<BufferStatisticsCollector>();
}

BufferManager::~BufferManager() {
    cleanup();
}

bool BufferManager::initialize(IResourceContext* resourceContext, 
                              BufferFactory* bufferFactory,
                              CommandExecutor* executor,
                              VkDeviceSize stagingSize) {
    this->resourceContext = resourceContext;
    this->bufferFactory = bufferFactory;
    this->executor = executor;
    
    if (!resourceContext || !bufferFactory) {
        std::cerr << "BufferManager: Invalid dependencies provided!" << std::endl;
        return false;
    }
    
    // Initialize staging pool
    if (!stagingPool->initialize(*resourceContext->getContext(), stagingSize)) {
        std::cerr << "Failed to initialize staging buffer pool!" << std::endl;
        return false;
    }
    
    // Initialize buffer registry
    if (!bufferRegistry->initialize(resourceContext, bufferFactory)) {
        std::cerr << "Failed to initialize buffer registry!" << std::endl;
        return false;
    }
    
    // Initialize transfer orchestrator
    if (!transferOrchestrator->initialize(stagingPool.get(), bufferRegistry.get(), executor)) {
        std::cerr << "Failed to initialize transfer orchestrator!" << std::endl;
        return false;
    }
    
    // Initialize statistics collector
    if (!statisticsCollector->initialize(stagingPool.get(), bufferRegistry.get(), transferOrchestrator.get())) {
        std::cerr << "Failed to initialize statistics collector!" << std::endl;
        return false;
    }
    
    return true;
}

void BufferManager::cleanup() {
    if (statisticsCollector) {
        statisticsCollector->cleanup();
    }
    if (transferOrchestrator) {
        transferOrchestrator->cleanup();
    }
    if (bufferRegistry) {
        bufferRegistry->cleanup();
    }
    if (stagingPool) {
        stagingPool->cleanup();
    }
    
    resourceContext = nullptr;
    bufferFactory = nullptr;
    executor = nullptr;
}

IResourceContext* BufferManager::getResourceContext() const {
    return resourceContext;
}

BufferFactory* BufferManager::getBufferFactory() const {
    return bufferFactory;
}

CommandExecutor* BufferManager::getCommandExecutor() const {
    return executor;
}

StagingBufferPool& BufferManager::getPrimaryStagingBuffer() {
    return *stagingPool;
}

const StagingBufferPool& BufferManager::getPrimaryStagingBuffer() const {
    return *stagingPool;
}

StagingBufferPool::StagingRegion BufferManager::allocateStaging(VkDeviceSize size, VkDeviceSize alignment) {
    return stagingPool->allocate(size, alignment);
}

StagingBufferPool::StagingRegionGuard BufferManager::allocateStagingGuarded(VkDeviceSize size, VkDeviceSize alignment) {
    return stagingPool->allocateGuarded(size, alignment);
}

void BufferManager::resetAllStaging() {
    stagingPool->reset();
}

std::unique_ptr<GPUBuffer> BufferManager::createBuffer(VkDeviceSize size,
                                                      VkBufferUsageFlags usage,
                                                      VkMemoryPropertyFlags properties) {
    auto buffer = std::make_unique<GPUBuffer>();
    
    if (!buffer->initialize(resourceContext, this, size, usage, properties)) {
        return nullptr;
    }
    
    // Register the buffer with our registry for tracking
    bufferRegistry->registerBuffer(buffer.get());
    
    return buffer;
}

bool BufferManager::uploadData(GPUBuffer& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (buffer.getMappedData()) {
        if (offset + size > buffer.getSize()) {
            return false;
        }
        memcpy(static_cast<char*>(buffer.getMappedData()) + offset, data, size);
        return true;
    }
    
    return buffer.addData(data, size);
}

void BufferManager::flushAllBuffers() {
    transferOrchestrator->flushAllBuffers();
}

bool BufferManager::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return transferOrchestrator->copyToBuffer(dst, data, size, offset);
}

bool BufferManager::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst,
                                      VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    return transferOrchestrator->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset);
}

CommandExecutor::AsyncTransfer BufferManager::copyToBufferAsync(const ResourceHandle& dst, const void* data,
                                                               VkDeviceSize size, VkDeviceSize offset) {
    return transferOrchestrator->copyToBufferAsync(dst, data, size, offset);
}

CommandExecutor::AsyncTransfer BufferManager::copyBufferToBufferAsync(const ResourceHandle& src, const ResourceHandle& dst,
                                                                     VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    return transferOrchestrator->copyBufferToBufferAsync(src, dst, size, srcOffset, dstOffset);
}

bool BufferManager::executeBatch(const TransferBatch& batch) {
    return transferOrchestrator->executeBatch(batch);
}

CommandExecutor::AsyncTransfer BufferManager::executeBatchAsync(const TransferBatch& batch) {
    return transferOrchestrator->executeBatchAsync(batch);
}

bool BufferManager::mapAndCopyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    return transferOrchestrator->mapAndCopyToBuffer(dst, data, size, offset);
}

bool BufferManager::tryOptimizeMemory() {
    return statisticsCollector->tryOptimizeMemory();
}

bool BufferManager::isTransferQueueAvailable() const {
    return transferOrchestrator->isTransferQueueAvailable();
}

void BufferManager::flushPendingTransfers() {
    transferOrchestrator->flushPendingTransfers();
}

BufferManager::BufferStats BufferManager::getStats() const {
    return statisticsCollector->getStats();
}

bool BufferManager::isUnderMemoryPressure() const {
    return statisticsCollector->isUnderMemoryPressure();
}

bool BufferManager::hasPendingStagingOperations() const {
    return statisticsCollector->hasPendingStagingOperations();
}