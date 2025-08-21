#include "gpu_buffer.h"
#include "buffer_manager.h"
#include "../managers/resource_context.h"
#include "../../core/vulkan_raii.h"
#include <cstring>
#include <iostream>

GPUBuffer::~GPUBuffer() {
    cleanup();
}

bool GPUBuffer::initialize(ResourceContext* resourceContext, BufferManager* bufferManager, VkDeviceSize size,
                          VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    this->resourceContext = resourceContext;
    this->bufferManager = bufferManager;
    this->bufferSize = size;
    this->isDeviceLocal = (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    
    if (isDeviceLocal) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createMappedBuffer(size, usage, properties)
        );
    } else {
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

void GPUBuffer::cleanup() {
    if (storageHandle && resourceContext) {
        resourceContext->destroyResource(*storageHandle);
        storageHandle.reset();
    }
    
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

bool GPUBuffer::addData(const void* data, VkDeviceSize size, VkDeviceSize alignment) {
    if (!storageHandle || !data) return false;
    
    if (storageHandle->mappedData) {
        memcpy(static_cast<char*>(storageHandle->mappedData) + stagingBytesWritten, data, size);
        stagingBytesWritten += size;
        return true;
    }
    
    if (!isDeviceLocal || !resourceContext) return false;
    
    if (!bufferManager) return false;
    
    auto stagingRegion = bufferManager->allocateStaging(size, alignment);
    
    if (!stagingRegion.mappedData) {
        bufferManager->resetAllStaging();
        stagingBytesWritten = 0;
        stagingStartOffset = 0;
        stagingRegion = bufferManager->allocateStaging(size, alignment);
    }
    
    if (stagingRegion.mappedData) {
        memcpy(stagingRegion.mappedData, data, size);
        if (stagingBytesWritten == 0) {
            stagingStartOffset = stagingRegion.offset;
        }
        stagingBytesWritten += size;
        needsUpload = true;
        return true;
    }
    
    return false;
}

void GPUBuffer::flushToGPU(VkDeviceSize dstOffset) {
    if (!needsUpload || stagingBytesWritten == 0 || !isDeviceLocal) {
        return;
    }
    
    if (!bufferManager) return;
    
    auto& stagingBuffer = bufferManager->getPrimaryStagingBuffer();
    
    ResourceHandle stagingHandle;
    stagingHandle.buffer = vulkan_raii::make_buffer(stagingBuffer.getBuffer(), resourceContext->getContext());
    stagingHandle.buffer.detach();
    
    resourceContext->copyBufferToBuffer(
        stagingHandle,
        *storageHandle,
        stagingBytesWritten,
        stagingStartOffset,
        dstOffset
    );
    
    resetStaging();
}

void GPUBuffer::resetStaging() {
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}