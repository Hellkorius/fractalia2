#include "transfer_manager.h"
#include "validation_utils.h"
#include "../buffers/transfer_orchestrator.h"

bool TransferManager::initialize(TransferOrchestrator* transferOrchestrator) {
    if (!ValidationUtils::validateDependencies("TransferManager::initialize", transferOrchestrator)) {
        return false;
    }
    
    this->transferOrchestrator = transferOrchestrator;
    initialized = true;
    return true;
}

void TransferManager::cleanup() {
    transferOrchestrator = nullptr;
    initialized = false;
}

bool TransferManager::copyToBuffer(const ResourceHandle& dst, const void* data, 
                                  VkDeviceSize size, VkDeviceSize offset) {
    if (!initialized || !transferOrchestrator) {
        ValidationUtils::logError("TransferManager", "copyToBuffer", "not initialized");
        return false;
    }
    
    return transferOrchestrator->copyToBuffer(dst, data, size, offset);
}

bool TransferManager::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst,
                                        VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!initialized || !transferOrchestrator) {
        ValidationUtils::logError("TransferManager", "copyBufferToBuffer", "not initialized");
        return false;
    }
    
    return transferOrchestrator->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset);
}

CommandExecutor::AsyncTransfer TransferManager::copyToBufferAsync(const ResourceHandle& dst, const void* data,
                                                                 VkDeviceSize size, VkDeviceSize offset) {
    if (!initialized || !transferOrchestrator) {
        ValidationUtils::logError("TransferManager", "copyToBufferAsync", "not initialized");
        return {};
    }
    
    return transferOrchestrator->copyToBufferAsync(dst, data, size, offset);
}