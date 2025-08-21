#pragma once

#include "buffer_operations_interface.h"
#include <vulkan/vulkan.h>
#include <vector>

// Forward declarations
class ResourceCoordinator;

/**
 * Shared service for buffer upload operations
 * Single responsibility: handle all buffer upload logic consistently
 */
class BufferUploadService {
public:
    BufferUploadService();
    ~BufferUploadService();
    
    bool initialize(ResourceCoordinator* resourceCoordinator);
    void cleanup();
    
    // Generic upload to any buffer implementing IBufferOperations
    template<typename BufferType>
    bool upload(BufferType& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0) {
        static_assert(std::is_base_of_v<IBufferOperations, BufferType>, 
                     "BufferType must implement IBufferOperations");
        
        if (!buffer.isInitialized()) {
            return false;
        }
        
        return buffer.copyData(data, size, offset);
    }
    
    // Batch upload operations for multiple buffers
    struct UploadOperation {
        IBufferOperations* buffer;
        const void* data;
        VkDeviceSize size;
        VkDeviceSize offset;
        
        UploadOperation(IBufferOperations* buf, const void* d, VkDeviceSize s, VkDeviceSize o = 0)
            : buffer(buf), data(d), size(s), offset(o) {}
    };
    
    bool uploadBatch(const std::vector<UploadOperation>& operations);
    
    // Upload with validation
    template<typename BufferType>
    bool uploadWithValidation(BufferType& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0) {
        if (!validateUpload(buffer, size, offset)) {
            return false;
        }
        return upload(buffer, data, size, offset);
    }
    
private:
    ResourceCoordinator* resourceCoordinator = nullptr;
    
    // Helper for validation
    bool validateUpload(const IBufferOperations& buffer, VkDeviceSize size, VkDeviceSize offset) const;
};