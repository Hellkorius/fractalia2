#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "../core/vulkan_context.h"

class ComputeDeviceInfo {
public:
    explicit ComputeDeviceInfo(VulkanContext* ctx);
    ~ComputeDeviceInfo() = default;

    bool initialize();
    
    glm::uvec3 getOptimalWorkgroupSize() const;
    uint32_t getMaxComputeWorkgroupInvocations() const;
    bool supportsSubgroupOperations() const;
    
    glm::uvec3 calculateOptimalWorkgroupSize(uint32_t dataSize,
                                            const glm::uvec3& maxWorkgroupSize = {1024, 1024, 64}) const;
    uint32_t calculateOptimalWorkgroupCount(uint32_t dataSize, uint32_t workgroupSize) const;
    
    const VkPhysicalDeviceProperties& getDeviceProperties() const { return deviceProperties_; }
    const VkPhysicalDeviceFeatures& getDeviceFeatures() const { return deviceFeatures_; }

private:
    VulkanContext* context_;
    VkPhysicalDeviceProperties deviceProperties_{};
    VkPhysicalDeviceFeatures deviceFeatures_{};
};