#include "gpu_buffer_manager.h"
#include "resource_context.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <vector>

// GPUBufferRing implementation
GPUBufferRing::~GPUBufferRing() {
    cleanup();
}

bool GPUBufferRing::initialize(ResourceContext* resourceContext, VkDeviceSize size,
                               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    this->resourceContext = resourceContext;
    this->bufferSize = size;
    this->isDeviceLocal = (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    
    // For device-local buffers, add transfer destination flag
    if (isDeviceLocal) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    
    // Create the buffer based on memory properties
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        // Host-visible buffer (can be mapped)
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createMappedBuffer(size, usage, properties)
        );
    } else {
        // Device-local buffer (cannot be mapped directly)
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createBuffer(size, usage, properties)
        );
    }
    
    if (!storageHandle->isValid()) {
        storageHandle.reset();
        return false;
    }
    
    return true;
}

void GPUBufferRing::cleanup() {
    if (storageHandle && resourceContext) {
        resourceContext->destroyResource(*storageHandle);
        storageHandle.reset();
    }
    
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

bool GPUBufferRing::addData(const void* data, VkDeviceSize size, VkDeviceSize alignment) {
    if (!storageHandle || !data) return false;
    
    // If buffer is host-visible, write directly
    if (storageHandle->mappedData) {
        memcpy(static_cast<char*>(storageHandle->mappedData) + stagingBytesWritten, data, size);
        stagingBytesWritten += size;
        return true;
    }
    
    // For device-local buffers, use staging buffer
    if (!isDeviceLocal || !resourceContext) return false;
    
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    auto stagingRegion = stagingBuffer.allocate(size, alignment);
    
    if (!stagingRegion.mappedData) {
        // Reset and try again
        stagingBuffer.reset();
        stagingBytesWritten = 0;
        stagingStartOffset = 0;
        stagingRegion = stagingBuffer.allocate(size, alignment);
    }
    
    if (stagingRegion.mappedData) {
        memcpy(stagingRegion.mappedData, data, size);
        if (stagingBytesWritten == 0) {
            // Track where our batch starts
            stagingStartOffset = stagingRegion.offset;
        }
        stagingBytesWritten += size;
        needsUpload = true;
        return true;
    }
    
    return false;
}

void GPUBufferRing::flushToGPU(VkDeviceSize dstOffset) {
    if (!needsUpload || stagingBytesWritten == 0 || !isDeviceLocal) {
        return;
    }
    
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    
    // Create a temporary ResourceHandle that doesn't own the buffer
    ResourceHandle stagingHandle;
    stagingHandle.buffer = vulkan_raii::make_buffer(stagingBuffer.getBuffer(), resourceContext->getContext());
    stagingHandle.buffer.detach(); // Release ownership - staging buffer manages its own lifecycle
    
    // Copy from staging buffer to device-local buffer
    resourceContext->copyBufferToBuffer(
        stagingHandle,
        *storageHandle,
        stagingBytesWritten,
        stagingStartOffset,
        dstOffset
    );
    
    // Reset staging state
    resetStaging();
}

void GPUBufferRing::resetStaging() {
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

// GPUBufferManager implementation
GPUBufferManager::GPUBufferManager() {
}

GPUBufferManager::~GPUBufferManager() {
    cleanup();
}

bool GPUBufferManager::initialize(ResourceContext* resourceContext, StagingBufferManager* stagingManager) {
    this->resourceContext = resourceContext;
    this->stagingManager = stagingManager;
    return true;
}

void GPUBufferManager::cleanup() {
    // Note: We don't manage the lifetime of individual GPUBufferRing objects,
    // they manage themselves. We just track them for statistics.
    managedBuffers.clear();
    resourceContext = nullptr;
    stagingManager = nullptr;
}

std::unique_ptr<GPUBufferRing> GPUBufferManager::createBuffer(VkDeviceSize size,
                                                             VkBufferUsageFlags usage,
                                                             VkMemoryPropertyFlags properties) {
    auto buffer = std::make_unique<GPUBufferRing>();
    
    if (!buffer->initialize(resourceContext, size, usage, properties)) {
        return nullptr;
    }
    
    // Register for tracking
    registerBuffer(buffer.get());
    
    return buffer;
}

bool GPUBufferManager::uploadData(GPUBufferRing& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    // For host-visible buffers, direct copy
    if (buffer.getMappedData()) {
        memcpy(static_cast<char*>(buffer.getMappedData()) + offset, data, size);
        return true;
    }
    
    // For device-local buffers, use staging
    return buffer.addData(data, size);
}

void GPUBufferManager::flushAll() {
    for (auto* buffer : managedBuffers) {
        if (buffer && buffer->hasPendingData()) {
            buffer->flushToGPU();
        }
    }
}

void GPUBufferManager::resetAllStaging() {
    for (auto* buffer : managedBuffers) {
        if (buffer) {
            buffer->resetStaging();
        }
    }
    
    if (stagingManager) {
        stagingManager->reset();
    }
}

GPUBufferManager::BufferStats GPUBufferManager::getStats() const {
    BufferStats stats;
    
    for (const auto* buffer : managedBuffers) {
        if (!buffer) continue;
        
        stats.totalBuffers++;
        stats.totalSize += buffer->getSize();
        
        if (buffer->getMappedData()) {
            stats.hostVisibleBuffers++;
        } else {
            stats.deviceLocalBuffers++;
            
            if (buffer->hasPendingData()) {
                stats.buffersWithPendingData++;
                // Note: We don't have direct access to staging bytes per buffer
                // This would need to be tracked if needed
            }
        }
    }
    
    return stats;
}

bool GPUBufferManager::hasPendingStagingOperations() const {
    for (const auto* buffer : managedBuffers) {
        if (buffer && buffer->hasPendingData()) {
            return true;
        }
    }
    return false;
}

void GPUBufferManager::registerBuffer(GPUBufferRing* buffer) {
    if (buffer) {
        managedBuffers.push_back(buffer);
    }
}

void GPUBufferManager::unregisterBuffer(GPUBufferRing* buffer) {
    managedBuffers.erase(
        std::remove(managedBuffers.begin(), managedBuffers.end(), buffer),
        managedBuffers.end()
    );
}