#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../../core/vulkan_raii.h"

// Resource handle combining buffer/image with allocation
struct ResourceHandle {
    vulkan_raii::Buffer buffer;
    vulkan_raii::Image image;
    vulkan_raii::ImageView imageView;
    vulkan_raii::DeviceMemory memory;
    void* mappedData = nullptr;
    VkDeviceSize size = 0;
    
    bool isValid() const { return buffer || image; }
};