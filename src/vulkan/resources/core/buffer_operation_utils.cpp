#include "buffer_operation_utils.h"
#include "validation_utils.h"
#include "command_executor.h"
#include <cstring>

bool BufferOperationUtils::copyBufferToBuffer(CommandExecutor* executor,
                                              const ResourceHandle& src, const ResourceHandle& dst,
                                              VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!validateBufferCopyOperation(src, dst, size, "BufferOperationUtils::copyBufferToBuffer")) {
        return false;
    }
    
    if (!executor) {
        ValidationUtils::logError("BufferOperationUtils", "copyBufferToBuffer", "null executor");
        return false;
    }
    
    if (!validateCopyParameters(size, srcOffset, dstOffset)) {
        return false;
    }
    
    executor->copyBufferToBuffer(src.buffer.get(), dst.buffer.get(), size, srcOffset, dstOffset);
    return true;
}

bool BufferOperationUtils::validateBufferCopyOperation(const ResourceHandle& src, const ResourceHandle& dst,
                                                      VkDeviceSize size, const std::string& context) {
    if (!src.isValid()) {
        ValidationUtils::logValidationFailure(context, "source buffer", "invalid resource handle");
        return false;
    }
    
    if (!dst.isValid()) {
        ValidationUtils::logValidationFailure(context, "destination buffer", "invalid resource handle");
        return false;
    }
    
    if (size == 0) {
        ValidationUtils::logValidationFailure(context, "copy size", "zero bytes");
        return false;
    }
    
    return true;
}

bool BufferOperationUtils::isBufferHostVisible(const ResourceHandle& buffer) {
    if (!buffer.isValid()) {
        return false;
    }
    
    // Check if buffer has host-visible memory properties
    // This would need to be implemented based on how memory properties are tracked
    // For now, check if buffer has mapped data as a proxy
    return buffer.mappedData != nullptr;
}

bool BufferOperationUtils::requiresStaging(const ResourceHandle& buffer) {
    return !isBufferHostVisible(buffer);
}

bool BufferOperationUtils::copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data,
                                                   VkDeviceSize size, VkDeviceSize offset) {
    if (!dst.isValid()) {
        ValidationUtils::logValidationFailure("BufferOperationUtils::copyDirectToMappedBuffer", 
                                             "destination buffer", "invalid resource handle");
        return false;
    }
    
    if (!data) {
        ValidationUtils::logValidationFailure("BufferOperationUtils::copyDirectToMappedBuffer",
                                             "source data", "null pointer");
        return false;
    }
    
    if (!dst.mappedData) {
        ValidationUtils::logValidationFailure("BufferOperationUtils::copyDirectToMappedBuffer",
                                             "destination buffer", "not mapped");
        return false;
    }
    
    if (size == 0) {
        return true; // No-op for zero-size copies
    }
    
    // Direct memory copy to mapped buffer
    std::memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
    return true;
}

bool BufferOperationUtils::validateCopyParameters(VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (size == 0) {
        ValidationUtils::logValidationFailure("BufferOperationUtils::validateCopyParameters",
                                             "copy size", "zero bytes");
        return false;
    }
    
    // Additional validation could be added here for offset bounds checking
    // if buffer sizes were tracked in ResourceHandle
    
    return true;
}