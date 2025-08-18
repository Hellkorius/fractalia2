#include "resource_context.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include "../../PolygonFactory.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <array>

// Simple VMA allocator replacement using manual allocation
// This provides the interface without requiring VMA dependency
struct VmaAllocator_Impl {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    const VulkanFunctionLoader* loader;
    
    struct Allocation {
        VkDeviceMemory memory;
        VkDeviceSize offset;
        VkDeviceSize size;
        void* mappedData;
    };
    
    std::vector<Allocation> allocations;
};

// StagingRingBuffer implementation
bool StagingRingBuffer::initialize(const VulkanContext& context, VkDeviceSize size) {
    this->context = &context;
    this->totalSize = size;
    this->currentOffset = 0;
    
    // Cache frequently used loader and device references for performance
    const auto& loader = context.getLoader();
    const VkDevice device = context.getDevice();
    
    // Create staging buffer - always host visible and coherent
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (loader.vkCreateBuffer(device, &bufferInfo, nullptr, &bufferHandle) != VK_SUCCESS) {
        std::cerr << "Failed to create staging ring buffer!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    loader.vkGetBufferMemoryRequirements(device, bufferHandle, &memRequirements);
    
    VkPhysicalDeviceMemoryProperties memProperties;
    loader.vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &memProperties);
    
    uint32_t memoryType = UINT32_MAX;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryType = i;
            break;
        }
    }
    
    if (memoryType == UINT32_MAX) {
        std::cerr << "Failed to find suitable memory type for staging buffer!" << std::endl;
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    VkDeviceMemory memory;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    if (loader.vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging buffer memory!" << std::endl;
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    if (loader.vkBindBufferMemory(device, bufferHandle, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind staging buffer memory!" << std::endl;
        loader.vkFreeMemory(device, memory, nullptr);
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    if (loader.vkMapMemory(device, memory, 0, size, 0, &ringBuffer.mappedData) != VK_SUCCESS) {
        std::cerr << "Failed to map staging buffer memory!" << std::endl;
        loader.vkFreeMemory(device, memory, nullptr);
        loader.vkDestroyBuffer(device, bufferHandle, nullptr);
        return false;
    }
    
    // Wrap handles in RAII wrappers
    ringBuffer.buffer = vulkan_raii::make_buffer(bufferHandle, &context);
    ringBuffer.memory = vulkan_raii::make_device_memory(memory, &context);
    ringBuffer.size = size;
    
    return true;
}

void StagingRingBuffer::cleanup() {
    if (context && ringBuffer.isValid()) {
        if (ringBuffer.mappedData && ringBuffer.memory) {
            context->getLoader().vkUnmapMemory(context->getDevice(), ringBuffer.memory.get());
        }
        
        // RAII wrappers will handle cleanup automatically
        ringBuffer.buffer.reset();
        ringBuffer.memory.reset();
        
        ringBuffer.mappedData = nullptr;
        ringBuffer.size = 0;
        currentOffset = 0;
        totalSize = 0;
    }
}

StagingRingBuffer::StagingRegion StagingRingBuffer::allocate(VkDeviceSize size, VkDeviceSize alignment) {
    // Align current offset
    VkDeviceSize alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
    
    // Check if we need to wrap around
    if (alignedOffset + size > totalSize) {
        alignedOffset = 0; // Wrap to beginning
        currentOffset = 0;
    }
    
    if (alignedOffset + size > totalSize) {
        std::cerr << "Staging buffer allocation too large: " << size << " bytes" << std::endl;
        return {};
    }
    
    StagingRegion region;
    region.buffer = ringBuffer.buffer.get();
    region.offset = alignedOffset;
    region.size = size;
    region.mappedData = static_cast<char*>(ringBuffer.mappedData) + alignedOffset;
    
    currentOffset = alignedOffset + size;
    
    return region;
}

void StagingRingBuffer::reset() {
    currentOffset = 0;
}

// GPUBufferRing implementation
GPUBufferRing::~GPUBufferRing() {
    cleanup();
}

bool GPUBufferRing::initialize(ResourceContext* resourceContext, VkDeviceSize size,
                               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    this->resourceContext = resourceContext;
    this->bufferSize = size;
    this->isDeviceLocal = (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    
    // For device-local buffers, add transfer destination flag
    if (isDeviceLocal) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    
    // Create the buffer based on memory properties
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        // Host-visible buffer (can be mapped)
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createMappedBuffer(size, usage, properties)
        );
    } else {
        // Device-local buffer (cannot be mapped directly)
        storageHandle = std::make_unique<ResourceHandle>(
            resourceContext->createBuffer(size, usage, properties)
        );
    }
    
    if (!storageHandle->isValid()) {
        storageHandle.reset();
        return false;
    }
    
    return true;
}

void GPUBufferRing::cleanup() {
    if (storageHandle && resourceContext) {
        resourceContext->destroyResource(*storageHandle);
        storageHandle.reset();
    }
    
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

bool GPUBufferRing::addData(const void* data, VkDeviceSize size, VkDeviceSize alignment) {
    if (!storageHandle || !data) return false;
    
    // If buffer is host-visible, write directly
    if (storageHandle->mappedData) {
        memcpy(static_cast<char*>(storageHandle->mappedData) + stagingBytesWritten, data, size);
        stagingBytesWritten += size;
        return true;
    }
    
    // For device-local buffers, use staging buffer
    if (!isDeviceLocal || !resourceContext) return false;
    
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    auto stagingRegion = stagingBuffer.allocate(size, alignment);
    
    if (!stagingRegion.mappedData) {
        // Reset and try again
        stagingBuffer.reset();
        stagingBytesWritten = 0;
        stagingStartOffset = 0;
        stagingRegion = stagingBuffer.allocate(size, alignment);
    }
    
    if (stagingRegion.mappedData) {
        memcpy(stagingRegion.mappedData, data, size);
        if (stagingBytesWritten == 0) {
            // Track where our batch starts
            stagingStartOffset = stagingRegion.offset;
        }
        stagingBytesWritten += size;
        needsUpload = true;
        return true;
    }
    
    return false;
}

void GPUBufferRing::flushToGPU(VkDeviceSize dstOffset) {
    if (!needsUpload || stagingBytesWritten == 0 || !isDeviceLocal) {
        return;
    }
    
    auto& stagingBuffer = resourceContext->getStagingBuffer();
    
    // Create a temporary ResourceHandle that doesn't own the buffer
    ResourceHandle stagingHandle;
    stagingHandle.buffer = vulkan_raii::make_buffer(stagingBuffer.getBuffer(), resourceContext->getContext());
    stagingHandle.buffer.detach(); // Release ownership - staging buffer manages its own lifecycle
    
    // Copy from staging buffer to device-local buffer
    resourceContext->copyBufferToBuffer(
        stagingHandle,
        *storageHandle,
        stagingBytesWritten,
        stagingStartOffset,
        dstOffset
    );
    
    // Reset staging state
    resetStaging();
}

void GPUBufferRing::resetStaging() {
    if (resourceContext) {
        resourceContext->getStagingBuffer().reset();
    }
    stagingBytesWritten = 0;
    stagingStartOffset = 0;
    needsUpload = false;
}

// ResourceContext implementation
ResourceContext::ResourceContext() {
}

ResourceContext::~ResourceContext() {
    cleanup();
}

bool ResourceContext::initialize(const VulkanContext& context, VkCommandPool commandPool) {
    this->context = &context;
    
    if (!initializeVMA()) {
        return false;
    }
    
    // Initialize staging buffer (16MB for large entity uploads)
    if (!stagingBuffer.initialize(context, 16 * 1024 * 1024)) {
        std::cerr << "Failed to initialize staging buffer!" << std::endl;
        return false;
    }
    
    // Initialize command executor if command pool is provided
    if (commandPool != VK_NULL_HANDLE) {
        if (!executor.initialize(context, commandPool)) {
            std::cerr << "Failed to initialize command executor!" << std::endl;
            return false;
        }
    }
    
    return true;
}

void ResourceContext::cleanup() {
    cleanupBeforeContextDestruction();
    
    // Run cleanup callbacks in reverse order
    for (auto it = cleanupCallbacks.rbegin(); it != cleanupCallbacks.rend(); ++it) {
        (*it)();
    }
    cleanupCallbacks.clear();
    
    executor.cleanup();
    stagingBuffer.cleanup();
    cleanupVMA();
    context = nullptr;
}

void ResourceContext::cleanupBeforeContextDestruction() {
    // Clean up graphics resources first
    for (auto& handle : uniformBufferHandles) {
        if (handle.isValid()) {
            destroyResource(handle);
        }
    }
    uniformBufferHandles.clear();
    uniformBuffers.clear();
    uniformBuffersMapped.clear();
    
    if (vertexBufferHandle.isValid()) {
        destroyResource(vertexBufferHandle);
    }
    if (indexBufferHandle.isValid()) {
        destroyResource(indexBufferHandle);
    }
    
    // RAII wrapper will handle cleanup automatically
    graphicsDescriptorPool.reset();
}

ResourceHandle ResourceContext::createBuffer(VkDeviceSize size, 
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
    
    uint32_t memoryType = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    VkDeviceMemory memory;
    if (context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory!" << std::endl;
        context->getLoader().vkDestroyBuffer(context->getDevice(), bufferHandle, nullptr);
        return {};
    }
    
    if (context->getLoader().vkBindBufferMemory(context->getDevice(), bufferHandle, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind buffer memory!" << std::endl;
        context->getLoader().vkFreeMemory(context->getDevice(), memory, nullptr);
        context->getLoader().vkDestroyBuffer(context->getDevice(), bufferHandle, nullptr);
        return {};
    }
    
    // Wrap handles in RAII wrappers
    handle.buffer = vulkan_raii::make_buffer(bufferHandle, context);
    handle.memory = vulkan_raii::make_device_memory(memory, context);
    handle.size = size;
    
    // Update stats
    memoryStats.totalAllocated += memRequirements.size;
    memoryStats.activeAllocations++;
    
    return handle;
}

ResourceHandle ResourceContext::createMappedBuffer(VkDeviceSize size,
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

ResourceHandle ResourceContext::createImage(uint32_t width, uint32_t height,
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
    
    uint32_t memoryType = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    
    VkDeviceMemory memory;
    if (context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate image memory!" << std::endl;
        context->getLoader().vkDestroyImage(context->getDevice(), imageHandle, nullptr);
        return {};
    }
    
    if (context->getLoader().vkBindImageMemory(context->getDevice(), imageHandle, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind image memory!" << std::endl;
        context->getLoader().vkFreeMemory(context->getDevice(), memory, nullptr);
        context->getLoader().vkDestroyImage(context->getDevice(), imageHandle, nullptr);
        return {};
    }
    
    // Wrap handles in RAII wrappers
    handle.image = vulkan_raii::make_image(imageHandle, context);
    handle.memory = vulkan_raii::make_device_memory(memory, context);
    handle.size = memRequirements.size;
    
    // Update stats
    memoryStats.totalAllocated += memRequirements.size;
    memoryStats.activeAllocations++;
    
    return handle;
}

ResourceHandle ResourceContext::createImageView(const ResourceHandle& imageHandle,
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

void ResourceContext::destroyResource(ResourceHandle& handle) {
    if (!context || !handle.isValid()) return;
    
    if (handle.mappedData && handle.memory) {
        context->getLoader().vkUnmapMemory(context->getDevice(), handle.memory.get());
    }
    
    // Update stats if we have memory to free
    if (handle.memory) {
        memoryStats.totalFreed += handle.size;
        memoryStats.activeAllocations--;
    }
    
    // RAII wrappers will handle cleanup automatically
    handle.imageView.reset();
    handle.buffer.reset();
    handle.image.reset();
    handle.memory.reset();
    
    handle.mappedData = nullptr;
    handle.size = 0;
}

void ResourceContext::copyToBuffer(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (dst.mappedData) {
        // Direct copy to mapped buffer - synchronous
        memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
    } else {
        // Handle large uploads by chunking if necessary
        const VkDeviceSize maxChunkSize = 8 * 1024 * 1024; // 8MB chunks
        VkDeviceSize remaining = size;
        VkDeviceSize currentOffset = 0;
        
        while (remaining > 0) {
            VkDeviceSize chunkSize = std::min(remaining, maxChunkSize);
            
            // Try to allocate staging region for this chunk
            auto stagingRegion = stagingBuffer.allocate(chunkSize);
            if (!stagingRegion.mappedData) {
                // Reset staging buffer and try again
                stagingBuffer.reset();
                stagingRegion = stagingBuffer.allocate(chunkSize);
            }
            
            if (stagingRegion.mappedData) {
                memcpy(stagingRegion.mappedData, static_cast<const char*>(data) + currentOffset, chunkSize);
                // Use synchronous transfer to ensure data is available before returning
                ResourceHandle stagingHandle;
                stagingHandle.buffer = vulkan_raii::make_buffer(stagingRegion.buffer, context);
                stagingHandle.buffer.detach(); // Don't own the buffer - staging buffer manages it
                stagingHandle.mappedData = stagingRegion.mappedData;
                stagingHandle.size = chunkSize;
                
                copyBufferToBuffer(stagingHandle, dst, chunkSize, stagingRegion.offset, offset + currentOffset);
                
                remaining -= chunkSize;
                currentOffset += chunkSize;
            } else {
                std::cerr << "ResourceContext::copyToBuffer: Failed to allocate staging buffer for chunk of size " << chunkSize << std::endl;
                break;
            }
        }
    }
}

void ResourceContext::copyBufferToBuffer(const ResourceHandle& src, const ResourceHandle& dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (!src.isValid() || !dst.isValid()) {
        std::cerr << "ResourceContext::copyBufferToBuffer: Invalid resource handles!" << std::endl;
        return;
    }
    
    if (!src.buffer || !dst.buffer) {
        std::cerr << "ResourceContext::copyBufferToBuffer: Invalid buffer handles!" << std::endl;
        return;
    }
    
    executor.copyBufferToBuffer(src.buffer.get(), dst.buffer.get(), size, srcOffset, dstOffset);
}

CommandExecutor::AsyncTransfer ResourceContext::copyToBufferAsync(const ResourceHandle& dst, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (dst.mappedData) {
        // Direct copy to mapped buffer - no async needed
        memcpy(static_cast<char*>(dst.mappedData) + offset, data, size);
        return {}; // Return empty transfer (already complete)
    } else {
        // Use staging buffer with async transfer
        auto stagingRegion = stagingBuffer.allocate(size);
        if (stagingRegion.mappedData) {
            memcpy(stagingRegion.mappedData, data, size);
            return executor.copyBufferToBufferAsync(
                stagingRegion.buffer, dst.buffer.get(), size, 
                stagingRegion.offset, offset
            );
        }
    }
    
    return {}; // Failed allocation
}

vulkan_raii::DescriptorPool ResourceContext::createDescriptorPool() {
    return createDescriptorPool(DescriptorPoolConfig{});
}

vulkan_raii::DescriptorPool ResourceContext::createDescriptorPool(const DescriptorPoolConfig& config) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    if (config.uniformBuffers > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, config.uniformBuffers});
    }
    if (config.storageBuffers > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, config.storageBuffers});
    }
    if (config.sampledImages > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, config.sampledImages});
    }
    if (config.storageImages > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config.storageImages});
    }
    if (config.samplers > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, config.samplers});
    }
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = config.allowFreeDescriptorSets ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = config.maxSets;
    
    return vulkan_raii::create_descriptor_pool(context, &poolInfo);
}

void ResourceContext::destroyDescriptorPool(VkDescriptorPool pool) {
    if (context && pool != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDescriptorPool(context->getDevice(), pool, nullptr);
    }
}

bool ResourceContext::initializeVMA() {
    // Simplified VMA initialization - just store device info
    allocator = new VmaAllocator_Impl;
    allocator->device = context->getDevice();
    allocator->physicalDevice = context->getPhysicalDevice();
    allocator->loader = &context->getLoader();
    
    return true;
}

void ResourceContext::cleanupVMA() {
    if (allocator) {
        // Clean up any remaining allocations
        for (auto& alloc : allocator->allocations) {
            if (alloc.mappedData) {
                allocator->loader->vkUnmapMemory(allocator->device, alloc.memory);
            }
            allocator->loader->vkFreeMemory(allocator->device, alloc.memory, nullptr);
        }
        
        delete allocator;
        allocator = nullptr;
    }
}

uint32_t ResourceContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    context->getLoader().vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

// Graphics pipeline resource implementations (moved from VulkanResources)
bool ResourceContext::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 2;
    
    uniformBufferHandles.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformBufferHandles[i] = createMappedBuffer(
            bufferSize, 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        if (!uniformBufferHandles[i].isValid()) {
            std::cerr << "Failed to create uniform buffer " << i << std::endl;
            return false;
        }
        
        // Maintain compatibility with existing API
        uniformBuffers[i] = uniformBufferHandles[i].buffer.get();
        uniformBuffersMapped[i] = uniformBufferHandles[i].mappedData;
    }
    
    return true;
}

bool ResourceContext::createTriangleBuffers() {
    PolygonMesh triangle = PolygonFactory::createTriangle();
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * triangle.vertices.size();
    
    // Create staging buffer
    ResourceHandle stagingHandle = createMappedBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    if (!stagingHandle.isValid()) {
        std::cerr << "Failed to create vertex staging buffer!" << std::endl;
        return false;
    }
    
    // Copy vertex data to staging buffer
    memcpy(stagingHandle.mappedData, triangle.vertices.data(), (size_t)vertexBufferSize);
    
    // Create device-local vertex buffer
    vertexBufferHandle = createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    if (!vertexBufferHandle.isValid()) {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        destroyResource(stagingHandle);
        return false;
    }
    
    // Copy from staging to device buffer
    copyBufferToBuffer(stagingHandle, vertexBufferHandle, vertexBufferSize);
    
    // Clean up staging buffer
    destroyResource(stagingHandle);
    
    // Create index buffer
    indexCount = static_cast<uint32_t>(triangle.indices.size());
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * triangle.indices.size();
    
    // Create index staging buffer
    ResourceHandle indexStagingHandle = createMappedBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    if (!indexStagingHandle.isValid()) {
        std::cerr << "Failed to create index staging buffer!" << std::endl;
        return false;
    }
    
    // Copy index data to staging buffer
    memcpy(indexStagingHandle.mappedData, triangle.indices.data(), (size_t)indexBufferSize);
    
    // Create device-local index buffer
    indexBufferHandle = createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    if (!indexBufferHandle.isValid()) {
        std::cerr << "Failed to create index buffer!" << std::endl;
        destroyResource(indexStagingHandle);
        return false;
    }
    
    // Copy from staging to device buffer
    copyBufferToBuffer(indexStagingHandle, indexBufferHandle, indexBufferSize);
    
    // Clean up staging buffer
    destroyResource(indexStagingHandle);
    
    return true;
}

bool ResourceContext::createGraphicsDescriptorPool(VkDescriptorSetLayout descriptorSetLayout) {
    DescriptorPoolConfig config;
    config.maxSets = 1024;
    config.uniformBuffers = 1024;
    config.storageBuffers = 0;
    config.sampledImages = 0;
    config.allowFreeDescriptorSets = true;
    
    graphicsDescriptorPool = createDescriptorPool(config);
    
    if (!graphicsDescriptorPool) {
        std::cerr << "Failed to create graphics descriptor pool!" << std::endl;
        return false;
    }
    
    return true;
}

bool ResourceContext::createGraphicsDescriptorSets(VkDescriptorSetLayout descriptorSetLayout) {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool.get();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    graphicsDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (context->getLoader().vkAllocateDescriptorSets(context->getDevice(), &allocInfo, graphicsDescriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate graphics descriptor sets!" << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Use helper for uniform buffer (binding 0)
        std::vector<VkDescriptorBufferInfo> bufferInfos = {bufferInfo};
        VulkanUtils::writeDescriptorSets(context->getDevice(), context->getLoader(), graphicsDescriptorSets[i], bufferInfos, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }
    
    return true;
}

bool ResourceContext::updateDescriptorSetsWithPositionBuffer(VkBuffer positionBuffer) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding (binding 0)
        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = uniformBuffers[i];
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Position buffer binding (binding 2)
        VkDescriptorBufferInfo positionBufferInfo{};
        positionBufferInfo.buffer = positionBuffer;
        positionBufferInfo.offset = 0;
        positionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Write both descriptors
        VkWriteDescriptorSet descriptorWrites[2] = {};
        
        // UBO write
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Position buffer write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &positionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 2, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

bool ResourceContext::updateDescriptorSetsWithPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding (binding 0)
        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = uniformBuffers[i];
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Current position buffer binding (binding 2)
        VkDescriptorBufferInfo currentPositionBufferInfo{};
        currentPositionBufferInfo.buffer = currentPositionBuffer;
        currentPositionBufferInfo.offset = 0;
        currentPositionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Target position buffer binding (binding 3)
        VkDescriptorBufferInfo targetPositionBufferInfo{};
        targetPositionBufferInfo.buffer = targetPositionBuffer;
        targetPositionBufferInfo.offset = 0;
        targetPositionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Write all three descriptors
        VkWriteDescriptorSet descriptorWrites[3] = {};
        
        // UBO write
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Current position buffer write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &currentPositionBufferInfo;
        
        // Target position buffer write
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[2].dstBinding = 3;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &targetPositionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 3, descriptorWrites, 0, nullptr);
    }
    
    return true;
}

bool ResourceContext::updateDescriptorSetsWithEntityAndPositionBuffers(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    // Validate inputs
    if (entityBuffer == VK_NULL_HANDLE) {
        std::cerr << "ResourceContext: ERROR - Entity buffer is null in updateDescriptorSetsWithEntityAndPositionBuffers" << std::endl;
        return false;
    }
    if (positionBuffer == VK_NULL_HANDLE) {
        std::cerr << "ResourceContext: ERROR - Position buffer is null in updateDescriptorSetsWithEntityAndPositionBuffers" << std::endl;
        return false;
    }
    if (graphicsDescriptorSets.empty()) {
        std::cerr << "ResourceContext: ERROR - Graphics descriptor sets not created yet" << std::endl;
        return false;
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO binding (binding 0)
        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = uniformBuffers[i];
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(glm::mat4) * 2;
        
        // Entity buffer binding (binding 1) - THE MISSING BINDING!
        VkDescriptorBufferInfo entityBufferInfo{};
        entityBufferInfo.buffer = entityBuffer;
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = VK_WHOLE_SIZE;
        
        // Position buffer binding (binding 2)
        VkDescriptorBufferInfo positionBufferInfo{};
        positionBufferInfo.buffer = positionBuffer;
        positionBufferInfo.offset = 0;
        positionBufferInfo.range = VK_WHOLE_SIZE;
        
        // Write all three descriptors
        VkWriteDescriptorSet descriptorWrites[3] = {};
        
        // UBO write (binding 0)
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboBufferInfo;
        
        // Entity buffer write (binding 1)
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &entityBufferInfo;
        
        // Position buffer write (binding 2)
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = graphicsDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &positionBufferInfo;
        
        context->getLoader().vkUpdateDescriptorSets(context->getDevice(), 3, descriptorWrites, 0, nullptr);
    }
    
    return true;
}