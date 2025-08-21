#include "buffer_upload_service.h"
#include "../../vulkan/resources/managers/resource_context.h"
#include <iostream>

BufferUploadService::BufferUploadService() {
}

BufferUploadService::~BufferUploadService() {
    cleanup();
}

bool BufferUploadService::initialize(ResourceContext* resourceContext) {
    this->resourceContext = resourceContext;
    
    if (!resourceContext) {
        std::cerr << "BufferUploadService: ResourceContext is null" << std::endl;
        return false;
    }
    
    std::cout << "BufferUploadService: Initialized successfully" << std::endl;
    return true;
}

void BufferUploadService::cleanup() {
    resourceContext = nullptr;
}

bool BufferUploadService::uploadBatch(const std::vector<UploadOperation>& operations) {
    if (!resourceContext) {
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