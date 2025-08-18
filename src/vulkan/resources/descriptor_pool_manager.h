#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../core/vulkan_raii.h"

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
        uint32_t maxSets = 1024;
        uint32_t uniformBuffers = 1024;
        uint32_t storageBuffers = 1024;
        uint32_t sampledImages = 1024;
        uint32_t storageImages = 512;
        uint32_t samplers = 512;
        bool allowFreeDescriptorSets = true;
        bool bindlessReady = false; // Future-proof for bindless
    };
    
    // Descriptor pool creation/destruction
    vulkan_raii::DescriptorPool createDescriptorPool(const DescriptorPoolConfig& config);
    vulkan_raii::DescriptorPool createDescriptorPool(); // Overload with default config
    void destroyDescriptorPool(VkDescriptorPool pool);

private:
    const VulkanContext* context = nullptr;
};