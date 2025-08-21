#include "resource_factory.h"
#include "validation_utils.h"
#include "../buffers/buffer_factory.h"

bool ResourceFactory::initialize(const VulkanContext& context, MemoryAllocator* memoryAllocator) {
    if (!ValidationUtils::validateDependencies("ResourceFactory::initialize", &context, memoryAllocator)) {
        return false;
    }
    
    bufferFactory = new BufferFactory();
    if (!bufferFactory->initialize(context, memoryAllocator)) {
        ValidationUtils::logInitializationError("ResourceFactory", "BufferFactory initialization failed");
        delete bufferFactory;
        bufferFactory = nullptr;
        return false;
    }
    
    initialized = true;
    return true;
}

void ResourceFactory::cleanup() {
    if (bufferFactory) {
        bufferFactory->cleanup();
        delete bufferFactory;
        bufferFactory = nullptr;
    }
    initialized = false;
}

ResourceHandle ResourceFactory::createBuffer(VkDeviceSize size, 
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties) {
    if (!initialized || !bufferFactory) {
        ValidationUtils::logError("ResourceFactory", "createBuffer", "not initialized");
        return {};
    }
    
    return bufferFactory->createBuffer(size, usage, properties);
}

ResourceHandle ResourceFactory::createMappedBuffer(VkDeviceSize size,
                                                   VkBufferUsageFlags usage,
                                                   VkMemoryPropertyFlags properties) {
    if (!initialized || !bufferFactory) {
        ValidationUtils::logError("ResourceFactory", "createMappedBuffer", "not initialized");
        return {};
    }
    
    return bufferFactory->createMappedBuffer(size, usage, properties);
}

ResourceHandle ResourceFactory::createImage(uint32_t width, uint32_t height,
                                           VkFormat format,
                                           VkImageUsageFlags usage,
                                           VkMemoryPropertyFlags properties,
                                           VkSampleCountFlagBits samples) {
    if (!initialized || !bufferFactory) {
        ValidationUtils::logError("ResourceFactory", "createImage", "not initialized");
        return {};
    }
    
    return bufferFactory->createImage(width, height, format, usage, properties, samples);
}

ResourceHandle ResourceFactory::createImageView(const ResourceHandle& imageHandle,
                                               VkFormat format,
                                               VkImageAspectFlags aspectFlags) {
    if (!initialized || !bufferFactory) {
        ValidationUtils::logError("ResourceFactory", "createImageView", "not initialized");
        return {};
    }
    
    return bufferFactory->createImageView(imageHandle, format, aspectFlags);
}

void ResourceFactory::destroyResource(ResourceHandle& handle) {
    if (!initialized || !bufferFactory) {
        return;
    }
    
    bufferFactory->destroyResource(handle);
}