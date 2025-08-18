#include "vulkan_raii.h"
#include "vulkan_context.h"
#include "vulkan_function_loader.h"

namespace vulkan_raii {

void ShaderModuleDeleter::operator()(VkShaderModule handle) {
    if (context && handle != VK_NULL_HANDLE && context->getDevice() != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyShaderModule(context->getDevice(), handle, nullptr);
    }
}

void PipelineDeleter::operator()(VkPipeline handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyPipeline(context->getDevice(), handle, nullptr);
    }
}

void PipelineLayoutDeleter::operator()(VkPipelineLayout handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyPipelineLayout(context->getDevice(), handle, nullptr);
    }
}

void DescriptorSetLayoutDeleter::operator()(VkDescriptorSetLayout handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDescriptorSetLayout(context->getDevice(), handle, nullptr);
    }
}

void DescriptorPoolDeleter::operator()(VkDescriptorPool handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDescriptorPool(context->getDevice(), handle, nullptr);
    }
}

void RenderPassDeleter::operator()(VkRenderPass handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyRenderPass(context->getDevice(), handle, nullptr);
    }
}

void SemaphoreDeleter::operator()(VkSemaphore handle) {
    if (context && handle != VK_NULL_HANDLE && context->getDevice() != VK_NULL_HANDLE) {
        context->getLoader().vkDestroySemaphore(context->getDevice(), handle, nullptr);
    }
}

void FenceDeleter::operator()(VkFence handle) {
    if (context && handle != VK_NULL_HANDLE && context->getDevice() != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyFence(context->getDevice(), handle, nullptr);
    }
}

void CommandPoolDeleter::operator()(VkCommandPool handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyCommandPool(context->getDevice(), handle, nullptr);
    }
}

void BufferDeleter::operator()(VkBuffer handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyBuffer(context->getDevice(), handle, nullptr);
    }
}

void ImageDeleter::operator()(VkImage handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyImage(context->getDevice(), handle, nullptr);
    }
}

void ImageViewDeleter::operator()(VkImageView handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyImageView(context->getDevice(), handle, nullptr);
    }
}

void DeviceMemoryDeleter::operator()(VkDeviceMemory handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkFreeMemory(context->getDevice(), handle, nullptr);
    }
}

void FramebufferDeleter::operator()(VkFramebuffer handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyFramebuffer(context->getDevice(), handle, nullptr);
    }
}

void PipelineCacheDeleter::operator()(VkPipelineCache handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyPipelineCache(context->getDevice(), handle, nullptr);
    }
}

} // namespace vulkan_raii