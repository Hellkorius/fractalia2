#include "buffer_registry.h"
#include "gpu_buffer.h"
#include "../core/resource_coordinator.h"
#include "buffer_factory.h"
#include <algorithm>

BufferRegistry::~BufferRegistry() {
    cleanup();
}

bool BufferRegistry::initialize(ResourceCoordinator* coordinator, BufferFactory* bufferFactory) {
    if (!coordinator || !bufferFactory) {
        return false;
    }
    
    this->coordinator = coordinator;
    this->bufferFactory = bufferFactory;
    
    return true;
}

void BufferRegistry::cleanup() {
    managedBuffers.clear();
    coordinator = nullptr;
    bufferFactory = nullptr;
}

std::unique_ptr<GPUBuffer> BufferRegistry::createBuffer(VkDeviceSize size,
                                                       VkBufferUsageFlags usage,
                                                       VkMemoryPropertyFlags properties) {
    // BufferRegistry no longer creates GPUBuffers directly
    // This is now handled by BufferManager
    return nullptr;
}

void BufferRegistry::registerBuffer(GPUBuffer* buffer) {
    if (!buffer) return;
    
    auto it = std::find(managedBuffers.begin(), managedBuffers.end(), buffer);
    if (it == managedBuffers.end()) {
        managedBuffers.push_back(buffer);
    }
}

void BufferRegistry::unregisterBuffer(GPUBuffer* buffer) {
    if (!buffer) return;
    
    auto it = std::find(managedBuffers.begin(), managedBuffers.end(), buffer);
    if (it != managedBuffers.end()) {
        managedBuffers.erase(it);
    }
}

BufferRegistry::RegistryStats BufferRegistry::getStats() const {
    RegistryStats stats;
    
    for (const auto* buffer : managedBuffers) {
        if (!buffer) continue;
        
        stats.totalBuffers++;
        stats.totalBufferSize += buffer->getSize();
        
        if (buffer->getMappedData()) {
            stats.hostVisibleBuffers++;
        } else {
            stats.deviceLocalBuffers++;
            
            if (buffer->hasPendingData()) {
                stats.buffersWithPendingData++;
            }
        }
    }
    
    return stats;
}

bool BufferRegistry::hasPendingOperations() const {
    for (const auto* buffer : managedBuffers) {
        if (buffer && buffer->hasPendingData()) {
            return true;
        }
    }
    return false;
}

std::vector<GPUBuffer*> BufferRegistry::getBuffersWithPendingData() const {
    std::vector<GPUBuffer*> pendingBuffers;
    for (auto* buffer : managedBuffers) {
        if (buffer && buffer->hasPendingData()) {
            pendingBuffers.push_back(buffer);
        }
    }
    return pendingBuffers;
}