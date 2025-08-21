#include "resource_context_bridge.h"
#include "resource_coordinator.h"
#include "resource_factory.h"
#include "../buffers/buffer_factory.h"

ResourceContextBridge::ResourceContextBridge(ResourceCoordinator* coordinator, CommandExecutor* executor)
    : coordinator(coordinator), executor(executor) {
}

const VulkanContext* ResourceContextBridge::getContext() const {
    return coordinator ? coordinator->getContext() : nullptr;
}

MemoryAllocator* ResourceContextBridge::getMemoryAllocator() const {
    return coordinator ? coordinator->getMemoryAllocator() : nullptr;
}

BufferFactory* ResourceContextBridge::getBufferFactory() const {
    if (!coordinator || !coordinator->getResourceFactory()) {
        return nullptr;
    }
    return coordinator->getResourceFactory()->getBufferFactory();
}

CommandExecutor* ResourceContextBridge::getCommandExecutor() const {
    return executor;
}

ResourceHandle ResourceContextBridge::createBuffer(VkDeviceSize size, 
                                                   VkBufferUsageFlags usage,
                                                   VkMemoryPropertyFlags properties) {
    return coordinator ? coordinator->createBuffer(size, usage, properties) : ResourceHandle{};
}

ResourceHandle ResourceContextBridge::createMappedBuffer(VkDeviceSize size,
                                                         VkBufferUsageFlags usage,
                                                         VkMemoryPropertyFlags properties) {
    return coordinator ? coordinator->createMappedBuffer(size, usage, properties) : ResourceHandle{};
}

bool ResourceContextBridge::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, 
                                               VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    return coordinator ? coordinator->copyBufferToBuffer(src, dst, size, srcOffset, dstOffset) : false;
}

void ResourceContextBridge::destroyResource(ResourceHandle& handle) {
    if (coordinator) {
        coordinator->destroyResource(handle);
    }
}