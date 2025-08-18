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

void QueryPoolDeleter::operator()(VkQueryPool handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyQueryPool(context->getDevice(), handle, nullptr);
    }
}

// Core context object deleters
void InstanceDeleter::operator()(VkInstance handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyInstance(handle, nullptr);
    }
}

void DeviceDeleter::operator()(VkDevice handle) {
    if (context && handle != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDevice(handle, nullptr);
    }
}

void SurfaceKHRDeleter::operator()(VkSurfaceKHR handle) {
    if (context && handle != VK_NULL_HANDLE && context->getInstance() != VK_NULL_HANDLE) {
        context->getLoader().vkDestroySurfaceKHR(context->getInstance(), handle, nullptr);
    }
}

void DebugUtilsMessengerEXTDeleter::operator()(VkDebugUtilsMessengerEXT handle) {
    if (context && handle != VK_NULL_HANDLE && context->getInstance() != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDebugUtilsMessengerEXT(context->getInstance(), handle, nullptr);
    }
}


// Direct creation factory functions

PipelineCache create_pipeline_cache(const VulkanContext* context, const VkPipelineCacheCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return PipelineCache();
    }
    
    VkPipelineCache handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreatePipelineCache(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return PipelineCache();
    }
    
    return make_pipeline_cache(handle, context);
}

Pipeline create_graphics_pipeline(const VulkanContext* context, VkPipelineCache pipelineCache, const VkGraphicsPipelineCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return Pipeline();
    }
    
    VkPipeline handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateGraphicsPipelines(
        context->getDevice(), pipelineCache, 1, createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return Pipeline();
    }
    
    return make_pipeline(handle, context);
}

Pipeline create_compute_pipeline(const VulkanContext* context, VkPipelineCache pipelineCache, const VkComputePipelineCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return Pipeline();
    }
    
    VkPipeline handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateComputePipelines(
        context->getDevice(), pipelineCache, 1, createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return Pipeline();
    }
    
    return make_pipeline(handle, context);
}

PipelineLayout create_pipeline_layout(const VulkanContext* context, const VkPipelineLayoutCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return PipelineLayout();
    }
    
    VkPipelineLayout handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreatePipelineLayout(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return PipelineLayout();
    }
    
    return make_pipeline_layout(handle, context);
}

RenderPass create_render_pass(const VulkanContext* context, const VkRenderPassCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return RenderPass();
    }
    
    VkRenderPass handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateRenderPass(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return RenderPass();
    }
    
    return make_render_pass(handle, context);
}

DescriptorSetLayout create_descriptor_set_layout(const VulkanContext* context, const VkDescriptorSetLayoutCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return DescriptorSetLayout();
    }
    
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateDescriptorSetLayout(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return DescriptorSetLayout();
    }
    
    return make_descriptor_set_layout(handle, context);
}

DescriptorPool create_descriptor_pool(const VulkanContext* context, const VkDescriptorPoolCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return DescriptorPool();
    }
    
    VkDescriptorPool handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateDescriptorPool(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return DescriptorPool();
    }
    
    return make_descriptor_pool(handle, context);
}

CommandPool create_command_pool(const VulkanContext* context, const VkCommandPoolCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return CommandPool();
    }
    
    VkCommandPool handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateCommandPool(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return CommandPool();
    }
    
    return make_command_pool(handle, context);
}

Fence create_fence(const VulkanContext* context, const VkFenceCreateInfo* createInfo) {
    if (!context || !createInfo) {
        return Fence();
    }
    
    VkFence handle = VK_NULL_HANDLE;
    VkResult result = context->getLoader().vkCreateFence(
        context->getDevice(), createInfo, nullptr, &handle);
    
    if (result != VK_SUCCESS) {
        return Fence();
    }
    
    return make_fence(handle, context);
}

} // namespace vulkan_raii