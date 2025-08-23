#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../frame_graph_types.h"
#include "../../core/vulkan_raii.h"
#include <unordered_map>
#include <string>
#include <chrono>
#include <vector>

// Forward declarations
class VulkanContext;
class GPUMemoryMonitor;

namespace FrameGraphResources {

// Resource types that can be managed by the frame graph
struct FrameGraphBuffer {
    vulkan_raii::Buffer buffer;
    vulkan_raii::DeviceMemory memory;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    bool isExternal = false; // Managed outside frame graph
    std::string debugName;
};

struct FrameGraphImage {
    vulkan_raii::Image image;
    vulkan_raii::ImageView view;
    vulkan_raii::DeviceMemory memory;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
    VkImageUsageFlags usage = 0;
    bool isExternal = false; // Managed outside frame graph
    std::string debugName;
};

// Union type for all frame graph resources
using FrameGraphResource = std::variant<FrameGraphBuffer, FrameGraphImage>;

// Resource cleanup tracking
struct ResourceCleanupInfo {
    std::chrono::steady_clock::time_point lastAccessTime;
    uint32_t accessCount = 0;
    ResourceCriticality criticality = ResourceCriticality::Flexible;
    bool canEvict = true;
};

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // Initialization
    bool initialize(const VulkanContext& context);
    void cleanup();
    void cleanupBeforeContextDestruction();

    // Optional monitoring integration
    void setMemoryMonitor(GPUMemoryMonitor* monitor) { memoryMonitor_ = monitor; }

    // Resource creation
    FrameGraphTypes::ResourceId createBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage);
    FrameGraphTypes::ResourceId createImage(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage);

    // External resource import
    FrameGraphTypes::ResourceId importExternalBuffer(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkBufferUsageFlags usage);
    FrameGraphTypes::ResourceId importExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent);

    // Resource access
    VkBuffer getBuffer(FrameGraphTypes::ResourceId id) const;
    VkImage getImage(FrameGraphTypes::ResourceId id) const;
    VkImageView getImageView(FrameGraphTypes::ResourceId id) const;

    // Resource helpers for barrier manager
    const FrameGraphBuffer* getBufferResource(FrameGraphTypes::ResourceId id) const;
    const FrameGraphImage* getImageResource(FrameGraphTypes::ResourceId id) const;

    // Resource lifecycle management
    void removeSwapchainResources();
    void reset(); // Clear for next frame

    // Memory management
    void performResourceCleanup();
    bool isMemoryPressureCritical() const;
    void evictNonCriticalResources();

    // Debug and telemetry
    void debugPrint() const;
    void logAllocationTelemetry() const;

private:
    // Resource creation helpers
    bool createVulkanBuffer(FrameGraphBuffer& buffer);
    bool createVulkanImage(FrameGraphImage& image);

    // Resource classification
    ResourceCriticality classifyResource(const FrameGraphBuffer& buffer) const;
    ResourceCriticality classifyResource(const FrameGraphImage& image) const;

    // Advanced allocation strategies
    bool tryAllocateWithStrategy(FrameGraphBuffer& buffer, ResourceCriticality criticality);
    bool tryAllocateWithStrategy(FrameGraphImage& image, ResourceCriticality criticality);

    // Memory allocation helpers
    uint32_t findAnyCompatibleMemoryType(uint32_t typeFilter) const;

    // Resource cleanup and memory management
    void updateResourceAccessTracking(FrameGraphTypes::ResourceId resourceId);
    void markResourceForEviction(FrameGraphTypes::ResourceId resourceId);
    std::vector<FrameGraphTypes::ResourceId> getEvictionCandidates();
    bool attemptResourceEviction(FrameGraphTypes::ResourceId resourceId);

    // State
    const VulkanContext* context_ = nullptr;
    GPUMemoryMonitor* memoryMonitor_ = nullptr;
    bool initialized_ = false;
    
    // Resource storage
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphResource> resources_;
    std::unordered_map<std::string, FrameGraphTypes::ResourceId> resourceNameMap_;
    FrameGraphTypes::ResourceId nextResourceId_ = 1;
    
    // Resource cleanup tracking
    std::unordered_map<FrameGraphTypes::ResourceId, ResourceCleanupInfo> resourceCleanupInfo_;

    // Resource allocation failure telemetry (moved from original frame_graph)
    struct AllocationTelemetry {
        uint32_t totalAttempts = 0;
        uint32_t successfulCreations = 0;
        uint32_t retriedCreations = 0;
        uint32_t fallbackAllocations = 0;
        uint32_t hostMemoryFallbacks = 0;
        uint32_t criticalResourceFailures = 0;
        
        void recordAttempt() { ++totalAttempts; }
        void recordSuccess(bool wasRetried, bool wasFallback, bool wasHostMemory) {
            ++successfulCreations;
            if (wasRetried) ++retriedCreations;
            if (wasFallback) ++fallbackAllocations;
            if (wasHostMemory) ++hostMemoryFallbacks;
        }
        void recordCriticalFailure() { ++criticalResourceFailures; }
        
        // Performance impact assessment
        float getRetryRate() const { return totalAttempts ? (float)retriedCreations / totalAttempts : 0.0f; }
        float getFallbackRate() const { return totalAttempts ? (float)fallbackAllocations / totalAttempts : 0.0f; }
        float getHostMemoryRate() const { return totalAttempts ? (float)hostMemoryFallbacks / totalAttempts : 0.0f; }
    } mutable allocationTelemetry_;
};

} // namespace FrameGraphResources