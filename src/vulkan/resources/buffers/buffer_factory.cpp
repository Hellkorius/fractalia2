#include "buffer_factory.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include "../../core/vulkan_constants.h"
#include "buffer_manager.h" // For complete StagingRingBuffer definition
#include "../core/command_executor.h"
#include <iostream>
#include <cstring>

BufferFactory::BufferFactory() {
}

BufferFactory::~BufferFactory() {
    cleanup();
}

bool BufferFactory::initialize(const VulkanContext& context, MemoryAllocator* memoryAllocator) {
    this->context = &context;
    this->memoryAllocator = memoryAllocator;
    return true;
}

void BufferFactory::cleanup() {
    cleanupBeforeContextDestruction();
    context = nullptr;
    memoryAllocator = nullptr;
    stagingBuffer = nullptr;
    executor = nullptr;
}

void BufferFactory::cleanupBeforeContextDestruction() {
    // BufferFactory doesn't hold RAII resources directly
    // Individual ResourceHandle objects are cleaned up by their owners
}

ResourceHandle BufferFactory::createBuffer(VkDeviceSize size, 
                                          VkBufferUsageFlags usage,
                                          VkMemoryPropertyFlags properties) {
    ResourceHandle handle;
    
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (context->getLoader().vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &bufferHandle) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer!" << std::endl;
        return {};
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    context->getLoader().vkGetBufferMemoryRequirements(context->getDevice(), bufferHandle, &memRequirements);
    
    auto allocation = memoryAllocator->allocateMemory(memRequirements, properties);
    if (allocation.memory == VK_NULL_HANDLE) {
        std::cerr << "Failed to allocate buffer memory!" << std::endl;
        context->getLoader().vkDestroyBuffer(context->getDevice(), bufferHandle, nullptr);
        return {};
    }
    
    if (context->getLoader().vkBindBufferMemory(context->getDevice(), bufferHandle, allocation.memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind buffer memory!" << std::endl;
        memoryAllocator->freeMemory(allocation);
        context->getLoader().vkDestroyBuffer(context->getDevice(), bufferHandle, nullptr);
        return {};
    }
    
    // Wrap handles in RAII wrappers
    handle.buffer = vulkan_raii::make_buffer(bufferHandle, context);
    handle.memory = vulkan_raii::make_device_memory(allocation.memory, context);
    handle.size = size;
    
    return handle;
}

ResourceHandle BufferFactory::createMappedBuffer(VkDeviceSize size,
                                                VkBufferUsageFlags usage,
                                                VkMemoryPropertyFlags properties) {
    ResourceHandle handle = createBuffer(size, usage, properties);
    
    if (handle.isValid() && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        if (context->getLoader().vkMapMemory(context->getDevice(), handle.memory.get(), 0, size, 0, &handle.mappedData) != VK_SUCCESS) {
            std::cerr << "Failed to map buffer memory!" << std::endl;
            destroyResource(handle);
            return {};
        }
    }
    
    return handle;
}

ResourceHandle BufferFactory::createImage(uint32_t width, uint32_t height,
                                         VkFormat format,
                                         VkImageUsageFlags usage,
                                         VkMemoryPropertyFlags properties,
                                         VkSampleCountFlagBits samples) {
    ResourceHandle handle;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = samples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkImage imageHandle = VK_NULL_HANDLE;
    if (context->getLoader().vkCreateImage(context->getDevice(), &imageInfo, nullptr, &imageHandle) != VK_SUCCESS) {
        std::cerr << "Failed to create image!" << std::endl;
        return {};
    }
    
    // Allocate memory for image
    VkMemoryRequirements memRequirements;
    context->getLoader().vkGetImageMemoryRequirements(context->getDevice(), imageHandle, &memRequirements);
    
    auto allocation = memoryAllocator->allocateMemory(memRequirements, properties);
    if (allocation.memory == VK_NULL_HANDLE) {
        std::cerr << "Failed to allocate image memory!" << std::endl;
        context->getLoader().vkDestroyImage(context->getDevice(), imageHandle, nullptr);
        return {};
    }
    
    if (context->getLoader().vkBindImageMemory(context->getDevice(), imageHandle, allocation.memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind image memory!" << std::endl;
        memoryAllocator->freeMemory(allocation);
        context->getLoader().vkDestroyImage(context->getDevice(), imageHandle, nullptr);
        return {};
    }
    
    // Wrap handles in RAII wrappers
    handle.image = vulkan_raii::make_image(imageHandle, context);
    handle.memory = vulkan_raii::make_device_memory(allocation.memory, context);
    handle.size = allocation.size;
    
    return handle;
}

ResourceHandle BufferFactory::createImageView(const ResourceHandle& imageHandle,
                                             VkFormat format,
                                             VkImageAspectFlags aspectFlags) {
    ResourceHandle handle; // Create new handle
    
    // Copy the image by wrapping the existing handle (without claiming ownership)
    handle.image = vulkan_raii::make_image(imageHandle.image.get(), context);
    handle.image.detach(); // Don't own the image - it's owned by imageHandle
    handle.memory = vulkan_raii::make_device_memory(imageHandle.memory.get(), context);
    handle.memory.detach(); // Don't own the memory - it's owned by imageHandle
    handle.size = imageHandle.size;
    
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = imageHandle.image.get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkImageView imageViewHandle = VK_NULL_HANDLE;
    if (context->getLoader().vkCreateImageView(context->getDevice(), &viewInfo, nullptr, &imageViewHandle) != VK_SUCCESS) {
        std::cerr << "Failed to create image view!" << std::endl;
        return {};
    }
    
    handle.imageView = vulkan_raii::make_image_view(imageViewHandle, context);
    
    return handle;
}

void BufferFactory::destroyResource(ResourceHandle& handle) {
    if (!context || !handle.isValid()) return;
    
    if (handle.mappedData && handle.memory) {
        context->getLoader().vkUnmapMemory(context->getDevice(), handle.memory.get());
    }
    
    // RAII wrappers will handle cleanup automatically
    handle.imageView.reset();
    handle.buffer.reset();
    handle.image.reset();
    handle.memory.reset();
    
    handle.mappedData = nullptr;
    handle.size = 0;
}

void BufferFactory::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (dst.mappedData) {
        // Direct copy to mapped buffer - synchronous
        memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
    } else if (stagingBuffer) {
        // Handle large uploads by chunking if necessary
        const VkDeviceSize maxChunkSize = MAX_CHUNK_SIZE;
        VkDeviceSize remaining = size;
        VkDeviceSize currentOffset = 0;
        
        while (remaining > 0) {
            VkDeviceSize chunkSize = std::min(remaining, maxChunkSize);
            
            // Try to allocate staging region for this chunk
            auto stagingRegion = stagingBuffer->allocate(chunkSize);
            if (!stagingRegion.mappedData) {
                // Reset staging buffer and try again
                stagingBuffer->reset();
                stagingRegion = stagingBuffer->allocate(chunkSize);
                
                // If still no space, try with smaller chunk
                if (!stagingRegion.mappedData && chunkSize > 1024) {
                    chunkSize = std::min(chunkSize / 2, static_cast<VkDeviceSize>(MEGABYTE)); // Try with 1MB max
                    stagingRegion = stagingBuffer->allocate(chunkSize);
                }
            }
            
            if (stagingRegion.mappedData) {
                memcpy(stagingRegion.mappedData, static_cast<const char*>(data) + currentOffset, chunkSize);
                
                // Create staging handle with proper ownership management
                ResourceHandle stagingHandle;
                stagingHandle.buffer = vulkan_raii::make_buffer(stagingRegion.buffer, context);
                stagingHandle.buffer.detach(); // Don't own the buffer - staging buffer manages it
                stagingHandle.mappedData = stagingRegion.mappedData;
                stagingHandle.size = chunkSize;
                
                // Synchronous transfer to ensure completion before moving to next chunk
                copyBufferToBuffer(stagingHandle, dst, chunkSize, stagingRegion.offset, offset + currentOffset);
                
                remaining -= chunkSize;
                currentOffset += chunkSize;
            } else {
                std::cerr << "BufferFactory::copyToBuffer: Failed to allocate staging buffer for chunk of size " 
                          << chunkSize << " bytes. Remaining: " << remaining << " bytes" << std::endl;
                // Break to prevent infinite loop
                break;
            }
        }
    }
}

void BufferFactory::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!src.isValid() || !dst.isValid()) {
        std::cerr << "BufferFactory::copyBufferToBuffer: Invalid resource handles!" << std::endl;
        return;
    }
    
    if (!src.buffer || !dst.buffer) {
        std::cerr << "BufferFactory::copyBufferToBuffer: Invalid buffer handles!" << std::endl;
        return;
    }
    
    if (executor) {
        executor->copyBufferToBuffer(src.buffer.get(), dst.buffer.get(), size, srcOffset, dstOffset);
    } else {
        std::cerr << "BufferFactory::copyBufferToBuffer: No command executor available!" << std::endl;
    }
}