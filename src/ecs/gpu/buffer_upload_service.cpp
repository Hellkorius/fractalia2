#include "buffer_upload_service.h"
#include "../../vulkan/resources/core/resource_coordinator.h"
#include <iostream>

BufferUploadService::BufferUploadService() {
}

BufferUploadService::~BufferUploadService() {
    cleanup();
}

bool BufferUploadService::initialize(ResourceCoordinator* resourceCoordinator) {
    this->resourceCoordinator = resourceCoordinator;
    
    if (!resourceCoordinator) {
        std::cerr << "BufferUploadService: ResourceCoordinator is null" << std::endl;
        return false;
    }
    
    std::cout << "BufferUploadService: Initialized successfully" << std::endl;
    return true;
}

void BufferUploadService::cleanup() {
    resourceCoordinator = nullptr;
}

bool BufferUploadService::uploadBatch(const std::vector<UploadOperation>& operations) {
    if (!resourceCoordinator) {
        std::cerr << "BufferUploadService: Not initialized" << std::endl;
        return false;
    }
    
    bool allSucceeded = true;
    
    for (const auto& op : operations) {
        if (!op.buffer || !op.buffer->isInitialized()) {
            std::cerr << "BufferUploadService: Invalid buffer in batch operation" << std::endl;
            allSucceeded = false;
            continue;
        }
        
        if (!validateUpload(*op.buffer, op.size, op.offset)) {
            std::cerr << "BufferUploadService: Validation failed for batch operation" << std::endl;
            allSucceeded = false;
            continue;
        }
        
        if (!op.buffer->copyData(op.data, op.size, op.offset)) {
            std::cerr << "BufferUploadService: Failed to upload data in batch operation" << std::endl;
            allSucceeded = false;
        }
    }
    
    return allSucceeded;
}

bool BufferUploadService::validateUpload(const IBufferOperations& buffer, VkDeviceSize size, VkDeviceSize offset) const {
    if (!buffer.isInitialized()) {
        std::cerr << "BufferUploadService: Buffer not initialized" << std::endl;
        return false;
    }
    
    if (offset + size > buffer.getSize()) {
        std::cerr << "BufferUploadService: Upload would exceed buffer size ("
                  << offset + size << " > " << buffer.getSize() << ")" << std::endl;
        return false;
    }
    
    if (size == 0) {
        std::cerr << "BufferUploadService: Cannot upload zero bytes" << std::endl;
        return false;
    }
    
    return true;
}

bool BufferUploadService::readback(VkBuffer buffer, void* data, VkDeviceSize size, VkDeviceSize offset) {
    // For debug readback, we need to implement a simple staging buffer approach
    // This is expensive and should only be used for debugging
    std::cerr << "BufferUploadService::readback: GPU readback not implemented yet - spatial map data not available" << std::endl;
    return false;
}