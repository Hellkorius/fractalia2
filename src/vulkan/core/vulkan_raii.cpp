#include "vulkan_raii.h"
#include "vulkan_context.h"
#include "vulkan_function_loader.h"

namespace vulkan_raii {

// Generic deleter template
template<typename VkType>
struct GenericDeleter : VulkanDeleter {
    using DeleteFunction = void(*)(const VulkanFunctionLoader&, VkDevice, VkType, const VkAllocationCallbacks*);
    using InstanceDeleteFunction = void(*)(const VulkanFunctionLoader&, VkInstance, VkType, const VkAllocationCallbacks*);
    
    DeleteFunction deleteFunc;
    InstanceDeleteFunction instanceDeleteFunc;
    bool requiresDeviceCheck;
    
    GenericDeleter(DeleteFunction func, bool deviceCheck = false) 
        : VulkanDeleter(), deleteFunc(func), instanceDeleteFunc(nullptr), requiresDeviceCheck(deviceCheck) {}
    
    GenericDeleter(InstanceDeleteFunction func) 
        : VulkanDeleter(), deleteFunc(nullptr), instanceDeleteFunc(func), requiresDeviceCheck(false) {}
    
    GenericDeleter(const VulkanContext* ctx, DeleteFunction func, bool deviceCheck = false) 
        : VulkanDeleter(ctx), deleteFunc(func), instanceDeleteFunc(nullptr), requiresDeviceCheck(deviceCheck) {}
    
    GenericDeleter(const VulkanContext* ctx, InstanceDeleteFunction func) 
        : VulkanDeleter(ctx), deleteFunc(nullptr), instanceDeleteFunc(func), requiresDeviceCheck(false) {}
    
    void operator()(VkType handle) {
        if (!context || handle == VK_NULL_HANDLE) {
            return;
        }
        
        if (instanceDeleteFunc) {
            if (context->getInstance() != VK_NULL_HANDLE) {
                instanceDeleteFunc(context->getLoader(), context->getInstance(), handle, nullptr);
            }
        } else if (deleteFunc) {
            if (!requiresDeviceCheck || context->getDevice() != VK_NULL_HANDLE) {
                deleteFunc(context->getLoader(), context->getDevice(), handle, nullptr);
            }
        }
    }
};

// Specialized deleter for VkDeviceMemory (uses vkFreeMemory)
struct DeviceMemoryDeleterImpl : VulkanDeleter {
    DeviceMemoryDeleterImpl() : VulkanDeleter() {}
    explicit DeviceMemoryDeleterImpl(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkDeviceMemory handle) {
        if (context && handle != VK_NULL_HANDLE) {
            context->getLoader().vkFreeMemory(context->getDevice(), handle, nullptr);
        }
    }
};

// Function pointer wrappers for deleteFunc parameters
static void destroyShaderModule(const VulkanFunctionLoader& loader, VkDevice device, VkShaderModule handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyShaderModule(device, handle, allocator);
}

static void destroyPipeline(const VulkanFunctionLoader& loader, VkDevice device, VkPipeline handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyPipeline(device, handle, allocator);
}

static void destroyPipelineLayout(const VulkanFunctionLoader& loader, VkDevice device, VkPipelineLayout handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyPipelineLayout(device, handle, allocator);
}

static void destroyDescriptorSetLayout(const VulkanFunctionLoader& loader, VkDevice device, VkDescriptorSetLayout handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyDescriptorSetLayout(device, handle, allocator);
}

static void destroyDescriptorPool(const VulkanFunctionLoader& loader, VkDevice device, VkDescriptorPool handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyDescriptorPool(device, handle, allocator);
}

static void destroyRenderPass(const VulkanFunctionLoader& loader, VkDevice device, VkRenderPass handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyRenderPass(device, handle, allocator);
}

static void destroySemaphore(const VulkanFunctionLoader& loader, VkDevice device, VkSemaphore handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroySemaphore(device, handle, allocator);
}

static void destroyFence(const VulkanFunctionLoader& loader, VkDevice device, VkFence handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyFence(device, handle, allocator);
}

static void destroyCommandPool(const VulkanFunctionLoader& loader, VkDevice device, VkCommandPool handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyCommandPool(device, handle, allocator);
}

static void destroyBuffer(const VulkanFunctionLoader& loader, VkDevice device, VkBuffer handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyBuffer(device, handle, allocator);
}

static void destroyImage(const VulkanFunctionLoader& loader, VkDevice device, VkImage handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyImage(device, handle, allocator);
}

static void destroyImageView(const VulkanFunctionLoader& loader, VkDevice device, VkImageView handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyImageView(device, handle, allocator);
}

static void destroyFramebuffer(const VulkanFunctionLoader& loader, VkDevice device, VkFramebuffer handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyFramebuffer(device, handle, allocator);
}

static void destroyPipelineCache(const VulkanFunctionLoader& loader, VkDevice device, VkPipelineCache handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyPipelineCache(device, handle, allocator);
}

static void destroyQueryPool(const VulkanFunctionLoader& loader, VkDevice device, VkQueryPool handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyQueryPool(device, handle, allocator);
}

static void destroyDevice(const VulkanFunctionLoader& loader, VkDevice device, VkDevice handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyDevice(handle, allocator);
}

// Instance-based destroy functions
static void destroyInstance(const VulkanFunctionLoader& loader, VkInstance instance, VkInstance handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyInstance(handle, allocator);
}

static void destroySurfaceKHR(const VulkanFunctionLoader& loader, VkInstance instance, VkSurfaceKHR handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroySurfaceKHR(instance, handle, allocator);
}

static void destroyDebugUtilsMessengerEXT(const VulkanFunctionLoader& loader, VkInstance instance, VkDebugUtilsMessengerEXT handle, const VkAllocationCallbacks* allocator) {
    loader.vkDestroyDebugUtilsMessengerEXT(instance, handle, allocator);
}

// Concrete deleter implementations using generic template
void ShaderModuleDeleter::operator()(VkShaderModule handle) {
    GenericDeleter<VkShaderModule> deleter(context, destroyShaderModule, true);
    deleter(handle);
}

void PipelineDeleter::operator()(VkPipeline handle) {
    GenericDeleter<VkPipeline> deleter(context, destroyPipeline);
    deleter(handle);
}

void PipelineLayoutDeleter::operator()(VkPipelineLayout handle) {
    GenericDeleter<VkPipelineLayout> deleter(context, destroyPipelineLayout);
    deleter(handle);
}

void DescriptorSetLayoutDeleter::operator()(VkDescriptorSetLayout handle) {
    GenericDeleter<VkDescriptorSetLayout> deleter(context, destroyDescriptorSetLayout);
    deleter(handle);
}

void DescriptorPoolDeleter::operator()(VkDescriptorPool handle) {
    GenericDeleter<VkDescriptorPool> deleter(context, destroyDescriptorPool);
    deleter(handle);
}

void RenderPassDeleter::operator()(VkRenderPass handle) {
    GenericDeleter<VkRenderPass> deleter(context, destroyRenderPass);
    deleter(handle);
}

void SemaphoreDeleter::operator()(VkSemaphore handle) {
    GenericDeleter<VkSemaphore> deleter(context, destroySemaphore, true);
    deleter(handle);
}

void FenceDeleter::operator()(VkFence handle) {
    GenericDeleter<VkFence> deleter(context, destroyFence, true);
    deleter(handle);
}

void CommandPoolDeleter::operator()(VkCommandPool handle) {
    GenericDeleter<VkCommandPool> deleter(context, destroyCommandPool);
    deleter(handle);
}

void BufferDeleter::operator()(VkBuffer handle) {
    GenericDeleter<VkBuffer> deleter(context, destroyBuffer);
    deleter(handle);
}

void ImageDeleter::operator()(VkImage handle) {
    GenericDeleter<VkImage> deleter(context, destroyImage);
    deleter(handle);
}

void ImageViewDeleter::operator()(VkImageView handle) {
    GenericDeleter<VkImageView> deleter(context, destroyImageView);
    deleter(handle);
}

void DeviceMemoryDeleter::operator()(VkDeviceMemory handle) {
    DeviceMemoryDeleterImpl deleter(context);
    deleter(handle);
}

void FramebufferDeleter::operator()(VkFramebuffer handle) {
    GenericDeleter<VkFramebuffer> deleter(context, destroyFramebuffer);
    deleter(handle);
}

void PipelineCacheDeleter::operator()(VkPipelineCache handle) {
    GenericDeleter<VkPipelineCache> deleter(context, destroyPipelineCache);
    deleter(handle);
}

void QueryPoolDeleter::operator()(VkQueryPool handle) {
    GenericDeleter<VkQueryPool> deleter(context, destroyQueryPool);
    deleter(handle);
}

// Core context object deleters
void InstanceDeleter::operator()(VkInstance handle) {
    GenericDeleter<VkInstance> deleter(context, destroyInstance);
    deleter(handle);
}

void DeviceDeleter::operator()(VkDevice handle) {
    GenericDeleter<VkDevice> deleter(context, destroyDevice);
    deleter(handle);
}

void SurfaceKHRDeleter::operator()(VkSurfaceKHR handle) {
    GenericDeleter<VkSurfaceKHR> deleter(context, destroySurfaceKHR);
    deleter(handle);
}

void DebugUtilsMessengerEXTDeleter::operator()(VkDebugUtilsMessengerEXT handle) {
    GenericDeleter<VkDebugUtilsMessengerEXT> deleter(context, destroyDebugUtilsMessengerEXT);
    deleter(handle);
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