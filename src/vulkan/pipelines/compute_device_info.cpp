#include "compute_device_info.h"
#include "../core/vulkan_function_loader.h"
#include <algorithm>
#include <iostream>

ComputeDeviceInfo::ComputeDeviceInfo(VulkanContext* ctx) : context_(ctx) {}

bool ComputeDeviceInfo::initialize() {
    if (!context_) {
        std::cerr << "ComputeDeviceInfo: Invalid VulkanContext" << std::endl;
        return false;
    }
    
    const auto& loader = context_->getLoader();
    loader.vkGetPhysicalDeviceProperties(context_->getPhysicalDevice(), &deviceProperties_);
    
    std::cout << "ComputeDeviceInfo initialized successfully" << std::endl;
    return true;
}

glm::uvec3 ComputeDeviceInfo::getOptimalWorkgroupSize() const {
    uint32_t maxWorkgroupSize = std::min(deviceProperties_.limits.maxComputeWorkGroupInvocations, 64u);
    return glm::uvec3(maxWorkgroupSize, 1, 1);
}

uint32_t ComputeDeviceInfo::getMaxComputeWorkgroupInvocations() const {
    return deviceProperties_.limits.maxComputeWorkGroupInvocations;
}

bool ComputeDeviceInfo::supportsSubgroupOperations() const {
    return true;
}

glm::uvec3 ComputeDeviceInfo::calculateOptimalWorkgroupSize(uint32_t dataSize,
                                                          const glm::uvec3& maxWorkgroupSize) const {
    glm::uvec3 optimal = getOptimalWorkgroupSize();
    
    optimal.x = std::min(optimal.x, maxWorkgroupSize.x);
    optimal.y = std::min(optimal.y, maxWorkgroupSize.y);
    optimal.z = std::min(optimal.z, maxWorkgroupSize.z);
    
    if (dataSize <= optimal.x * 4) {
        optimal.x = std::min(optimal.x, (dataSize + 3) / 4);
        optimal.y = 1;
        optimal.z = 1;
    }
    
    return optimal;
}

uint32_t ComputeDeviceInfo::calculateOptimalWorkgroupCount(uint32_t dataSize, uint32_t workgroupSize) const {
    return (dataSize + workgroupSize - 1) / workgroupSize;
}