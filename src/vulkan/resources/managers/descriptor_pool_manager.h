#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../../core/vulkan_raii.h"
#include "../../core/vulkan_constants.h"

class VulkanContext;

// Descriptor pool management
class DescriptorPoolManager {
public:
    DescriptorPoolManager();
    ~DescriptorPoolManager();
    
    bool initialize(const VulkanContext& context);
    void cleanup();
    
    // Context access
    const VulkanContext* getContext() const { return context; }
    
    // Descriptor pool configuration
    struct DescriptorPoolConfig {
        uint32_t maxSets = DEFAULT_MAX_DESCRIPTOR_SETS;
        uint32_t uniformBuffers = DEFAULT_MAX_DESCRIPTOR_SETS;
        uint32_t storageBuffers = DEFAULT_MAX_DESCRIPTOR_SETS;
        uint32_t sampledImages = DEFAULT_MAX_DESCRIPTOR_SETS;
        uint32_t storageImages = DEFAULT_COMPUTE_CACHE_SIZE;
        uint32_t samplers = DEFAULT_COMPUTE_CACHE_SIZE;
        bool allowFreeDescriptorSets = true;
        bool bindlessReady = true;  // Enable for new unified descriptor indexing system
    };
    
    // Descriptor pool creation/destruction
    vulkan_raii::DescriptorPool createDescriptorPool(const DescriptorPoolConfig& config);
    vulkan_raii::DescriptorPool createDescriptorPool(); // Overload with default config
    void destroyDescriptorPool(VkDescriptorPool pool);

private:
    const VulkanContext* context = nullptr;
};