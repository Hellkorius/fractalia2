#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "resource_handle.h"

class CommandExecutor;

// Centralized buffer operations to eliminate duplication between BufferFactory and TransferOrchestrator
class BufferOperationUtils {
public:
    // Buffer copy operations - centralized logic
    static bool copyBufferToBuffer(CommandExecutor* executor,
                                  const ResourceHandle& src, const ResourceHandle& dst,
                                  VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    
    // Buffer validation for copy operations
    static bool validateBufferCopyOperation(const ResourceHandle& src, const ResourceHandle& dst,
                                           VkDeviceSize size, const std::string& context);
    
    // Memory property checks
    static bool isBufferHostVisible(const ResourceHandle& buffer);
    static bool requiresStaging(const ResourceHandle& buffer);
    
    // Direct memory copy for host-visible buffers
    static bool copyDirectToMappedBuffer(const ResourceHandle& dst, const void* data,
                                        VkDeviceSize size, VkDeviceSize offset);

private:
    // Internal validation helpers
    static bool validateCopyParameters(VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset);
};