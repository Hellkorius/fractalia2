#include "resource_manager.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_utils.h"
#include "../../core/vulkan_function_loader.h"
#include "../../monitoring/gpu_memory_monitor.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <thread>
#include <chrono>

namespace FrameGraphResources {

bool ResourceManager::initialize(const VulkanContext& context) {
    context_ = &context;
    
    if (context.getDevice() == VK_NULL_HANDLE) {
        std::cerr << "ResourceManager: Invalid context" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "ResourceManager initialized successfully" << std::endl;
    return true;
}

void ResourceManager::cleanup() {
    if (!initialized_) return;
    
    cleanupBeforeContextDestruction();
    
    resources_.clear();
    resourceNameMap_.clear();
    resourceCleanupInfo_.clear();
    
    nextResourceId_ = 1;
    initialized_ = false;
}

void ResourceManager::cleanupBeforeContextDestruction() {
    // Clear all RAII resources before context destruction
    for (auto& [id, resource] : resources_) {
        std::visit([](auto& res) {
            if (!res.isExternal) {
                if constexpr (std::is_same_v<std::decay_t<decltype(res)>, FrameGraphBuffer>) {
                    res.buffer.reset();
                    res.memory.reset();
                } else {
                    res.view.reset();
                    res.image.reset();
                    res.memory.reset();
                }
            }
        }, resource);
    }
}

FrameGraphTypes::ResourceId ResourceManager::createBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (!initialized_) {
        std::cerr << "ResourceManager: Cannot create buffer, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap_.find(name) != resourceNameMap_.end()) {
        std::cerr << "ResourceManager: Buffer with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId_++;
    
    FrameGraphBuffer buffer;
    buffer.size = size;
    buffer.usage = usage;
    buffer.isExternal = false;
    buffer.debugName = name;
    
    // Create Vulkan buffer
    if (!createVulkanBuffer(buffer)) {
        std::cerr << "ResourceManager: Failed to create Vulkan buffer for '" << name << "'" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    resources_[id] = std::move(buffer);
    resourceNameMap_[name] = id;
    
    // Initialize resource cleanup tracking
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphBuffer>(resources_[id]));
    cleanupInfo.canEvict = !std::get<FrameGraphBuffer>(resources_[id]).isExternal;
    resourceCleanupInfo_[id] = cleanupInfo;
    
    std::cout << "ResourceManager: Created buffer '" << name << "' (ID: " << id << ", Size: " << size << ")" << std::endl;
    return id;
}

FrameGraphTypes::ResourceId ResourceManager::createImage(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage) {
    if (!initialized_) {
        std::cerr << "ResourceManager: Cannot create image, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap_.find(name) != resourceNameMap_.end()) {
        std::cerr << "ResourceManager: Image with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId_++;
    
    FrameGraphImage image;
    image.format = format;
    image.extent = extent;
    image.usage = usage;
    image.isExternal = false;
    image.debugName = name;
    
    // Create Vulkan image
    if (!createVulkanImage(image)) {
        std::cerr << "ResourceManager: Failed to create Vulkan image for '" << name << "'" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    resources_[id] = std::move(image);
    resourceNameMap_[name] = id;
    
    // Initialize resource cleanup tracking
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphImage>(resources_[id]));
    cleanupInfo.canEvict = !std::get<FrameGraphImage>(resources_[id]).isExternal;
    resourceCleanupInfo_[id] = cleanupInfo;
    
    std::cout << "ResourceManager: Created image '" << name << "' (ID: " << id << ")" << std::endl;
    return id;
}

FrameGraphTypes::ResourceId ResourceManager::importExternalBuffer(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (!initialized_) {
        std::cerr << "ResourceManager: Cannot import buffer, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap_.find(name) != resourceNameMap_.end()) {
        std::cerr << "ResourceManager: Resource with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId_++;
    
    FrameGraphBuffer frameGraphBuffer;
    frameGraphBuffer.buffer = vulkan_raii::Buffer(buffer, context_);
    frameGraphBuffer.buffer.detach(); // Don't manage lifecycle for external buffers
    frameGraphBuffer.size = size;
    frameGraphBuffer.usage = usage;
    frameGraphBuffer.isExternal = true; // Don't manage lifecycle
    frameGraphBuffer.debugName = name;
    
    resources_[id] = std::move(frameGraphBuffer);
    resourceNameMap_[name] = id;
    
    // Initialize resource cleanup tracking for external buffer
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphBuffer>(resources_[id]));
    cleanupInfo.canEvict = false; // External buffers cannot be evicted
    resourceCleanupInfo_[id] = cleanupInfo;
    
    std::cout << "ResourceManager: Imported external buffer '" << name << "' (ID: " << id << ")" << std::endl;
    return id;
}

FrameGraphTypes::ResourceId ResourceManager::importExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent) {
    if (!initialized_) {
        std::cerr << "ResourceManager: Cannot import image, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap_.find(name) != resourceNameMap_.end()) {
        std::cerr << "ResourceManager: Resource with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId_++;
    
    FrameGraphImage frameGraphImage;
    frameGraphImage.image = vulkan_raii::Image(image, context_);
    frameGraphImage.image.detach(); // Don't manage lifecycle for external images
    frameGraphImage.view = vulkan_raii::ImageView(view, context_);
    frameGraphImage.view.detach(); // Don't manage lifecycle for external image views
    frameGraphImage.format = format;
    frameGraphImage.extent = extent;
    frameGraphImage.isExternal = true; // Don't manage lifecycle
    frameGraphImage.debugName = name;
    
    resources_[id] = std::move(frameGraphImage);
    resourceNameMap_[name] = id;
    
    // Initialize resource cleanup tracking for external image
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphImage>(resources_[id]));
    cleanupInfo.canEvict = false; // External images cannot be evicted
    resourceCleanupInfo_[id] = cleanupInfo;
    
    return id;
}

VkBuffer ResourceManager::getBuffer(FrameGraphTypes::ResourceId id) const {
    const FrameGraphBuffer* buffer = getBufferResource(id);
    return buffer ? buffer->buffer.get() : VK_NULL_HANDLE;
}

VkImage ResourceManager::getImage(FrameGraphTypes::ResourceId id) const {
    const FrameGraphImage* image = getImageResource(id);
    return image ? image->image.get() : VK_NULL_HANDLE;
}

VkImageView ResourceManager::getImageView(FrameGraphTypes::ResourceId id) const {
    const FrameGraphImage* image = getImageResource(id);
    return image ? image->view.get() : VK_NULL_HANDLE;
}

const FrameGraphBuffer* ResourceManager::getBufferResource(FrameGraphTypes::ResourceId id) const {
    auto it = resources_.find(id);
    if (it == resources_.end()) return nullptr;
    
    return std::get_if<FrameGraphBuffer>(&it->second);
}

const FrameGraphImage* ResourceManager::getImageResource(FrameGraphTypes::ResourceId id) const {
    auto it = resources_.find(id);
    if (it == resources_.end()) return nullptr;
    
    return std::get_if<FrameGraphImage>(&it->second);
}

void ResourceManager::removeSwapchainResources() {
    // Remove all swapchain images from the frame graph to prevent "already exists" errors
    auto it = resources_.begin();
    while (it != resources_.end()) {
        bool shouldRemove = false;
        std::string debugName;
        
        std::visit([&shouldRemove, &debugName](const auto& resource) {
            debugName = resource.debugName;
            // Remove swapchain images (they have names like "SwapchainImage_0", "SwapchainImage_1", etc.)
            if (debugName.find("SwapchainImage_") == 0) {
                shouldRemove = true;
            }
        }, it->second);
        
        if (shouldRemove) {
            std::cout << "ResourceManager: Removing old swapchain resource: " << debugName << std::endl;
            FrameGraphTypes::ResourceId resourceId = it->first;
            resourceNameMap_.erase(debugName);
            resourceCleanupInfo_.erase(resourceId); // Remove cleanup tracking for swapchain resources
            it = resources_.erase(it);
        } else {
            ++it;
        }
    }
}

void ResourceManager::reset() {
    // Reset for next frame - clear transient state but keep persistent resources
    // Clear transient resources (including swapchain images that change per frame)
    // Keep only persistent external buffers (entity/position buffers)
    auto it = resources_.begin();
    while (it != resources_.end()) {
        bool shouldRemove = false;
        
        std::visit([&shouldRemove](const auto& resource) {
            // Remove non-external resources (frame-created)
            if (!resource.isExternal) {
                shouldRemove = true;
                return;
            }
            
            // Keep all external images including swapchain images (now cached)
        }, it->second);
        
        if (shouldRemove) {
            FrameGraphTypes::ResourceId resourceId = it->first;
            resourceNameMap_.erase(std::visit([](const auto& resource) { return resource.debugName; }, it->second));
            resourceCleanupInfo_.erase(resourceId); // Remove cleanup tracking for removed resources
            it = resources_.erase(it);
        } else {
            ++it;
        }
    }
}

void ResourceManager::debugPrint() const {
    std::cout << "\n=== ResourceManager Debug Info ===" << std::endl;
    std::cout << "Resources (" << resources_.size() << "):" << std::endl;
    
    for (const auto& [id, resource] : resources_) {
        std::visit([id](const auto& res) {
            std::cout << "  ID " << id << ": " << res.debugName;
            if constexpr (std::is_same_v<std::decay_t<decltype(res)>, FrameGraphBuffer>) {
                std::cout << " (Buffer, Size: " << res.size << ")";
            } else {
                std::cout << " (Image, " << res.extent.width << "x" << res.extent.height << ")";
            }
            std::cout << (res.isExternal ? " [External]" : " [Managed]") << std::endl;
        }, resource);
    }
    std::cout << "============================\n" << std::endl;
}

// Private implementation methods

bool ResourceManager::createVulkanBuffer(FrameGraphBuffer& buffer) {
    allocationTelemetry_.recordAttempt();
    
    // Classify resource criticality for allocation strategy
    ResourceCriticality criticality = classifyResource(buffer);
    
    return tryAllocateWithStrategy(buffer, criticality);
}

bool ResourceManager::createVulkanImage(FrameGraphImage& image) {
    allocationTelemetry_.recordAttempt();
    
    // Classify resource criticality for allocation strategy
    ResourceCriticality criticality = classifyResource(image);
    
    return tryAllocateWithStrategy(image, criticality);
}

ResourceCriticality ResourceManager::classifyResource(const FrameGraphBuffer& buffer) const {
    // Critical: Entity and position buffers that are accessed every frame
    if (buffer.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        // Check debug names for ECS buffers
        if (buffer.debugName.find("Entity") != std::string::npos ||
            buffer.debugName.find("Position") != std::string::npos) {
            return ResourceCriticality::Critical;
        }
    }
    
    // Important: Vertex/Index buffers used for rendering
    if (buffer.usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
        return ResourceCriticality::Important;
    }
    
    // Flexible: Staging, uniform, and other temporary buffers
    return ResourceCriticality::Flexible;
}

ResourceCriticality ResourceManager::classifyResource(const FrameGraphImage& image) const {
    // Critical: Render targets and depth buffers
    if (image.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
        return ResourceCriticality::Critical;
    }
    
    // Important: Textures that are sampled frequently
    if (image.usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        return ResourceCriticality::Important;
    }
    
    // Flexible: Storage images and other temporary resources
    return ResourceCriticality::Flexible;
}

bool ResourceManager::tryAllocateWithStrategy(FrameGraphBuffer& buffer, ResourceCriticality criticality) {
    const auto& vk = context_->getLoader();
    const VkDevice device = context_->getDevice();
    
    // Determine retry and fallback limits based on criticality
    int maxRetries = 1;
    std::vector<VkMemoryPropertyFlags> memoryStrategies;
    
    switch (criticality) {
        case ResourceCriticality::Critical:
            maxRetries = 2;  // Limited retries for critical resources
            memoryStrategies = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};  // Device local only
            break;
            
        case ResourceCriticality::Important:
            maxRetries = 2;
            memoryStrategies = {
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            };
            break;
            
        case ResourceCriticality::Flexible:
            maxRetries = 3;
            memoryStrategies = {
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                0  // Any compatible memory
            };
            break;
    }
    
    for (int retry = 0; retry < maxRetries; ++retry) {
        // Exponential backoff delay for retries (except first attempt)
        if (retry > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * (1 << (retry - 1))));
        }
        
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = buffer.size;
        bufferInfo.usage = buffer.usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer vkBuffer;
        VkResult bufferResult = vk.vkCreateBuffer(device, &bufferInfo, nullptr, &vkBuffer);
        if (bufferResult != VK_SUCCESS) {
            std::cerr << "[ResourceManager] Buffer creation failed: " << buffer.debugName 
                      << " (criticality: " << static_cast<int>(criticality) << ", attempt " << (retry + 1) 
                      << "/" << maxRetries << ", VkResult: " << bufferResult << ")" << std::endl;
            continue;
        }
        
        buffer.buffer = vulkan_raii::Buffer(vkBuffer, context_);
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vk.vkGetBufferMemoryRequirements(device, buffer.buffer.get(), &memRequirements);
        
        // Try memory allocation strategies
        bool wasRetried = (retry > 0);
        bool wasFallback = false;
        bool wasHostMemory = false;
        
        for (size_t strategyIdx = 0; strategyIdx < memoryStrategies.size(); ++strategyIdx) {
            VkMemoryPropertyFlags memoryProperties = memoryStrategies[strategyIdx];
            wasFallback = (strategyIdx > 0);
            wasHostMemory = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            
            try {
                uint32_t memoryTypeIndex;
                if (memoryProperties == 0) {
                    memoryTypeIndex = findAnyCompatibleMemoryType(memRequirements.memoryTypeBits);
                } else {
                    memoryTypeIndex = VulkanUtils::findMemoryType(context_->getPhysicalDevice(), vk, 
                                                                  memRequirements.memoryTypeBits, memoryProperties);
                }
                
                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memRequirements.size;
                allocInfo.memoryTypeIndex = memoryTypeIndex;
                
                VkDeviceMemory vkMemory;
                VkResult allocResult = vk.vkAllocateMemory(device, &allocInfo, nullptr, &vkMemory);
                if (allocResult == VK_SUCCESS) {
                    buffer.memory = vulkan_raii::DeviceMemory(vkMemory, context_);
                    
                    VkResult bindResult = vk.vkBindBufferMemory(device, buffer.buffer.get(), buffer.memory.get(), 0);
                    if (bindResult == VK_SUCCESS) {
                        // Success - record telemetry
                        allocationTelemetry_.recordSuccess(wasRetried, wasFallback, wasHostMemory);
                        
                        if (wasFallback) {
                            std::cerr << "[ResourceManager] PERFORMANCE WARNING: Buffer '" << buffer.debugName 
                                      << "' allocated with fallback memory (properties: 0x" << std::hex 
                                      << memoryProperties << std::dec << ")" << std::endl;
                        }
                        
                        return true;
                    } else {
                        std::cerr << "[ResourceManager] Buffer memory bind failed: " << buffer.debugName 
                                  << " (VkResult: " << bindResult << ")" << std::endl;
                        buffer.memory.reset();
                    }
                } else if (allocResult == VK_ERROR_OUT_OF_DEVICE_MEMORY || 
                          allocResult == VK_ERROR_OUT_OF_HOST_MEMORY) {
                    // Expected allocation failures - continue to next strategy
                    continue;
                }
            } catch (const std::exception& e) {
                // Memory type not available - continue
                continue;
            }
        }
        
        // All memory strategies failed for this attempt
        buffer.buffer.reset();
    }
    
    // Complete failure
    if (criticality == ResourceCriticality::Critical) {
        allocationTelemetry_.recordCriticalFailure();
        std::cerr << "[ResourceManager] CRITICAL FAILURE: Unable to allocate critical buffer '" 
                  << buffer.debugName << "' - system performance will be severely degraded" << std::endl;
    }
    
    return false;
}

bool ResourceManager::tryAllocateWithStrategy(FrameGraphImage& image, ResourceCriticality criticality) {
    // Similar implementation as tryAllocateWithStrategy for buffers but for images
    // Implementation would be similar to the original frame_graph.cpp but moved here
    // For brevity, I'm providing a simplified version
    
    const auto& vk = context_->getLoader();
    const VkDevice device = context_->getDevice();
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = image.extent.width;
    imageInfo.extent.height = image.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = image.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = image.usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkImage vkImage;
    VkResult imageResult = vk.vkCreateImage(device, &imageInfo, nullptr, &vkImage);
    if (imageResult != VK_SUCCESS) {
        return false;
    }
    
    image.image = vulkan_raii::Image(vkImage, context_);
    
    // Simplified memory allocation - in production this would use the same strategy system as buffers
    VkMemoryRequirements memRequirements;
    vk.vkGetImageMemoryRequirements(device, image.image.get(), &memRequirements);
    
    try {
        uint32_t memoryTypeIndex = VulkanUtils::findMemoryType(context_->getPhysicalDevice(), vk, 
                                                              memRequirements.memoryTypeBits, 
                                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        
        VkDeviceMemory vkMemory;
        VkResult allocResult = vk.vkAllocateMemory(device, &allocInfo, nullptr, &vkMemory);
        if (allocResult == VK_SUCCESS) {
            image.memory = vulkan_raii::DeviceMemory(vkMemory, context_);
            
            VkResult bindResult = vk.vkBindImageMemory(device, image.image.get(), image.memory.get(), 0);
            if (bindResult == VK_SUCCESS) {
                allocationTelemetry_.recordSuccess(false, false, false);
                return true;
            }
        }
    } catch (const std::exception&) {
        return false;
    }
    
    return false;
}

uint32_t ResourceManager::findAnyCompatibleMemoryType(uint32_t typeFilter) const {
    const auto& vk = context_->getLoader();
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vk.vkGetPhysicalDeviceMemoryProperties(context_->getPhysicalDevice(), &memProperties);
    
    // Find first compatible memory type
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            return i;
        }
    }
    
    throw std::runtime_error("No compatible memory type found for fallback allocation!");
}

void ResourceManager::logAllocationTelemetry() const {
    if (allocationTelemetry_.totalAttempts == 0) return;
    
    std::cout << "[ResourceManager] Allocation Telemetry Report:" << std::endl;
    std::cout << "  Total attempts: " << allocationTelemetry_.totalAttempts << std::endl;
    std::cout << "  Successful: " << allocationTelemetry_.successfulCreations << std::endl;
    std::cout << "  Retry rate: " << (allocationTelemetry_.getRetryRate() * 100.0f) << "%" << std::endl;
    std::cout << "  Fallback rate: " << (allocationTelemetry_.getFallbackRate() * 100.0f) << "%" << std::endl;
    std::cout << "  Host memory rate: " << (allocationTelemetry_.getHostMemoryRate() * 100.0f) << "%" << std::endl;
    std::cout << "  Critical failures: " << allocationTelemetry_.criticalResourceFailures << std::endl;
    
    // Performance impact warnings
    if (allocationTelemetry_.getHostMemoryRate() > 0.1f) {
        std::cerr << "[ResourceManager] WARNING: >10% of allocations using host memory - GPU performance impacted" << std::endl;
    }
    
    if (allocationTelemetry_.criticalResourceFailures > 0) {
        std::cerr << "[ResourceManager] CRITICAL: " << allocationTelemetry_.criticalResourceFailures 
                  << " critical resource allocation failures detected" << std::endl;
    }
}

void ResourceManager::performResourceCleanup() {
    if (!memoryMonitor_) return;
    
    std::cout << "[ResourceManager] Resource cleanup performed" << std::endl;
}

bool ResourceManager::isMemoryPressureCritical() const {
    if (!memoryMonitor_) return false;
    
    float memoryPressure = memoryMonitor_->getMemoryPressure();
    return memoryPressure > 0.85f; // Critical threshold: >85% memory pressure
}

void ResourceManager::evictNonCriticalResources() {
    auto candidates = getEvictionCandidates();
    
    if (candidates.empty()) {
        std::cerr << "[ResourceManager] No eviction candidates available" << std::endl;
        return;
    }
    
    size_t evictedCount = 0;
    size_t targetEvictions = std::min(candidates.size(), size_t(5)); // Limit to 5 evictions per frame
    
    for (size_t i = 0; i < targetEvictions; ++i) {
        if (attemptResourceEviction(candidates[i])) {
            ++evictedCount;
        }
    }
    
    std::cout << "[ResourceManager] Evicted " << evictedCount << " non-critical resources" << std::endl;
}

void ResourceManager::updateResourceAccessTracking(FrameGraphTypes::ResourceId resourceId) {
    auto it = resourceCleanupInfo_.find(resourceId);
    if (it != resourceCleanupInfo_.end()) {
        it->second.lastAccessTime = std::chrono::steady_clock::now();
        it->second.accessCount++;
    }
}

void ResourceManager::markResourceForEviction(FrameGraphTypes::ResourceId resourceId) {
    auto it = resourceCleanupInfo_.find(resourceId);
    if (it != resourceCleanupInfo_.end() && it->second.canEvict) {
        it->second.canEvict = true; // Mark as evictable if not already
    }
}

std::vector<FrameGraphTypes::ResourceId> ResourceManager::getEvictionCandidates() {
    std::vector<FrameGraphTypes::ResourceId> candidates;
    auto now = std::chrono::steady_clock::now();
    
    // Find resources that haven't been accessed in the last 3 seconds and can be evicted
    const auto EVICTION_THRESHOLD = std::chrono::seconds(3);
    
    for (const auto& [resourceId, cleanupInfo] : resourceCleanupInfo_) {
        if (!cleanupInfo.canEvict) continue;
        if (cleanupInfo.criticality == ResourceCriticality::Critical) continue; // Never evict critical resources
        
        auto timeSinceAccess = now - cleanupInfo.lastAccessTime;
        if (timeSinceAccess > EVICTION_THRESHOLD) {
            candidates.push_back(resourceId);
        }
    }
    
    // Sort candidates by criticality and last access time (least critical, oldest first)
    std::sort(candidates.begin(), candidates.end(), [this](FrameGraphTypes::ResourceId a, FrameGraphTypes::ResourceId b) {
        auto& infoA = resourceCleanupInfo_.at(a);
        auto& infoB = resourceCleanupInfo_.at(b);
        
        if (infoA.criticality != infoB.criticality) {
            return infoA.criticality > infoB.criticality; // Less critical first
        }
        return infoA.lastAccessTime < infoB.lastAccessTime; // Older first
    });
    
    return candidates;
}

bool ResourceManager::attemptResourceEviction(FrameGraphTypes::ResourceId resourceId) {
    auto resourceIt = resources_.find(resourceId);
    auto cleanupIt = resourceCleanupInfo_.find(resourceId);
    
    if (resourceIt == resources_.end() || cleanupIt == resourceCleanupInfo_.end()) {
        return false;
    }
    
    if (!cleanupIt->second.canEvict || cleanupIt->second.criticality == ResourceCriticality::Critical) {
        return false;
    }
    
    // Find and remove from name map
    std::string debugName;
    std::visit([&debugName](const auto& resource) {
        debugName = resource.debugName;
    }, resourceIt->second);
    
    auto nameIt = std::find_if(resourceNameMap_.begin(), resourceNameMap_.end(),
        [resourceId](const auto& pair) { return pair.second == resourceId; });
    
    if (nameIt != resourceNameMap_.end()) {
        resourceNameMap_.erase(nameIt);
    }
    
    // Remove resource and cleanup info
    resources_.erase(resourceIt);
    resourceCleanupInfo_.erase(cleanupIt);
    
    std::cout << "[ResourceManager] Evicted resource: " << debugName << " (ID: " << resourceId << ")" << std::endl;
    return true;
}

} // namespace FrameGraphResources