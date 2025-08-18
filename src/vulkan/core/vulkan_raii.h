#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <utility>

class VulkanContext;

namespace vulkan_raii {

// Forward declarations for deleters
struct VulkanDeleter {
    const VulkanContext* context;
    
    // Default constructor
    VulkanDeleter() : context(nullptr) {}
    
    explicit VulkanDeleter(const VulkanContext* ctx) : context(ctx) {}
    
    // Move constructor
    VulkanDeleter(VulkanDeleter&& other) noexcept : context(other.context) {
        other.context = nullptr;
    }
    
    // Move assignment
    VulkanDeleter& operator=(VulkanDeleter&& other) noexcept {
        if (this != &other) {
            context = other.context;
            other.context = nullptr;
        }
        return *this;
    }
    
    // No copy
    VulkanDeleter(const VulkanDeleter&) = delete;
    VulkanDeleter& operator=(const VulkanDeleter&) = delete;
};

// Base RAII wrapper template
template<typename VkType, typename Deleter>
class VulkanHandle {
private:
    VkType handle_;
    Deleter deleter_;

public:
    // Default constructor (creates null handle)
    VulkanHandle() : handle_(VK_NULL_HANDLE), deleter_(nullptr) {}
    
    // Constructor
    VulkanHandle(VkType handle, const VulkanContext* context) 
        : handle_(handle), deleter_(context) {}
    
    // Destructor
    ~VulkanHandle() {
        if (handle_ != VK_NULL_HANDLE && deleter_.context) {
            deleter_(handle_);
        }
    }
    
    // Move constructor
    VulkanHandle(VulkanHandle&& other) noexcept 
        : handle_(other.handle_), deleter_(std::move(other.deleter_)) {
        other.handle_ = VK_NULL_HANDLE;
    }
    
    // Move assignment
    VulkanHandle& operator=(VulkanHandle&& other) noexcept {
        if (this != &other) {
            // Clean up current resource
            if (handle_ != VK_NULL_HANDLE && deleter_.context) {
                deleter_(handle_);
            }
            
            // Move from other
            handle_ = other.handle_;
            deleter_ = std::move(other.deleter_);
            other.handle_ = VK_NULL_HANDLE;
        }
        return *this;
    }
    
    // No copy
    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;
    
    // Access
    VkType get() const { return handle_; }
    VkType operator*() const { return handle_; }
    operator VkType() const { return handle_; }
    operator bool() const { return handle_ != VK_NULL_HANDLE; }
    
    // Release ownership
    VkType release() {
        VkType temp = handle_;
        handle_ = VK_NULL_HANDLE;
        return temp;
    }
    
    // Reset
    void reset(VkType new_handle = VK_NULL_HANDLE) {
        if (handle_ != VK_NULL_HANDLE && deleter_.context) {
            deleter_(handle_);
        }
        handle_ = new_handle;
    }
    
    // Set context (for default-constructed handles)
    void setContext(const VulkanContext* context) {
        deleter_.context = context;
    }
    
    // Manually release handle to prevent auto-destruction
    void detach() {
        deleter_.context = nullptr;
    }
};

// Specific deleters for different Vulkan object types
struct ShaderModuleDeleter : VulkanDeleter {
    ShaderModuleDeleter() : VulkanDeleter() {}
    explicit ShaderModuleDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkShaderModule handle);
};

struct PipelineDeleter : VulkanDeleter {
    explicit PipelineDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkPipeline handle);
};

struct PipelineLayoutDeleter : VulkanDeleter {
    explicit PipelineLayoutDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkPipelineLayout handle);
};

struct DescriptorSetLayoutDeleter : VulkanDeleter {
    explicit DescriptorSetLayoutDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkDescriptorSetLayout handle);
};

struct DescriptorPoolDeleter : VulkanDeleter {
    explicit DescriptorPoolDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkDescriptorPool handle);
};

struct RenderPassDeleter : VulkanDeleter {
    explicit RenderPassDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkRenderPass handle);
};

struct SemaphoreDeleter : VulkanDeleter {
    explicit SemaphoreDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkSemaphore handle);
};

struct FenceDeleter : VulkanDeleter {
    explicit FenceDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkFence handle);
};

struct CommandPoolDeleter : VulkanDeleter {
    explicit CommandPoolDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkCommandPool handle);
};

struct BufferDeleter : VulkanDeleter {
    explicit BufferDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkBuffer handle);
};

struct ImageDeleter : VulkanDeleter {
    explicit ImageDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkImage handle);
};

struct ImageViewDeleter : VulkanDeleter {
    explicit ImageViewDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkImageView handle);
};

struct DeviceMemoryDeleter : VulkanDeleter {
    explicit DeviceMemoryDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkDeviceMemory handle);
};

struct FramebufferDeleter : VulkanDeleter {
    explicit FramebufferDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkFramebuffer handle);
};

struct PipelineCacheDeleter : VulkanDeleter {
    explicit PipelineCacheDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkPipelineCache handle);
};

struct QueryPoolDeleter : VulkanDeleter {
    explicit QueryPoolDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkQueryPool handle);
};

// Core context object deleters
struct InstanceDeleter : VulkanDeleter {
    explicit InstanceDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkInstance handle);
};

struct DeviceDeleter : VulkanDeleter {
    explicit DeviceDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkDevice handle);
};

struct SurfaceKHRDeleter : VulkanDeleter {
    explicit SurfaceKHRDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkSurfaceKHR handle);
};

struct DebugUtilsMessengerEXTDeleter : VulkanDeleter {
    explicit DebugUtilsMessengerEXTDeleter(const VulkanContext* ctx) : VulkanDeleter(ctx) {}
    
    void operator()(VkDebugUtilsMessengerEXT handle);
};


// Type aliases for convenience
using ShaderModule = VulkanHandle<VkShaderModule, ShaderModuleDeleter>;
using Pipeline = VulkanHandle<VkPipeline, PipelineDeleter>;
using PipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>;
using DescriptorSetLayout = VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>;
using DescriptorPool = VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter>;
using RenderPass = VulkanHandle<VkRenderPass, RenderPassDeleter>;
using Semaphore = VulkanHandle<VkSemaphore, SemaphoreDeleter>;
using Fence = VulkanHandle<VkFence, FenceDeleter>;
using CommandPool = VulkanHandle<VkCommandPool, CommandPoolDeleter>;
using Buffer = VulkanHandle<VkBuffer, BufferDeleter>;
using Image = VulkanHandle<VkImage, ImageDeleter>;
using ImageView = VulkanHandle<VkImageView, ImageViewDeleter>;
using DeviceMemory = VulkanHandle<VkDeviceMemory, DeviceMemoryDeleter>;
using Framebuffer = VulkanHandle<VkFramebuffer, FramebufferDeleter>;
using PipelineCache = VulkanHandle<VkPipelineCache, PipelineCacheDeleter>;
using QueryPool = VulkanHandle<VkQueryPool, QueryPoolDeleter>;

// Core context object types
using Instance = VulkanHandle<VkInstance, InstanceDeleter>;
using Device = VulkanHandle<VkDevice, DeviceDeleter>;
using SurfaceKHR = VulkanHandle<VkSurfaceKHR, SurfaceKHRDeleter>;
using DebugUtilsMessengerEXT = VulkanHandle<VkDebugUtilsMessengerEXT, DebugUtilsMessengerEXTDeleter>;

// Factory functions for creating RAII wrappers
template<typename VkType, typename Deleter>
VulkanHandle<VkType, Deleter> make_handle(VkType handle, const VulkanContext* context) {
    return VulkanHandle<VkType, Deleter>(handle, context);
}

// Convenience factory functions
inline ShaderModule make_shader_module(VkShaderModule handle, const VulkanContext* context) {
    return make_handle<VkShaderModule, ShaderModuleDeleter>(handle, context);
}

inline Pipeline make_pipeline(VkPipeline handle, const VulkanContext* context) {
    return make_handle<VkPipeline, PipelineDeleter>(handle, context);
}

inline PipelineLayout make_pipeline_layout(VkPipelineLayout handle, const VulkanContext* context) {
    return make_handle<VkPipelineLayout, PipelineLayoutDeleter>(handle, context);
}

inline DescriptorSetLayout make_descriptor_set_layout(VkDescriptorSetLayout handle, const VulkanContext* context) {
    return make_handle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>(handle, context);
}

inline Semaphore make_semaphore(VkSemaphore handle, const VulkanContext* context) {
    return make_handle<VkSemaphore, SemaphoreDeleter>(handle, context);
}

inline Fence make_fence(VkFence handle, const VulkanContext* context) {
    return make_handle<VkFence, FenceDeleter>(handle, context);
}

inline RenderPass make_render_pass(VkRenderPass handle, const VulkanContext* context) {
    return make_handle<VkRenderPass, RenderPassDeleter>(handle, context);
}

inline DescriptorPool make_descriptor_pool(VkDescriptorPool handle, const VulkanContext* context) {
    return make_handle<VkDescriptorPool, DescriptorPoolDeleter>(handle, context);
}

inline PipelineCache make_pipeline_cache(VkPipelineCache handle, const VulkanContext* context) {
    return make_handle<VkPipelineCache, PipelineCacheDeleter>(handle, context);
}

inline QueryPool make_query_pool(VkQueryPool handle, const VulkanContext* context) {
    return make_handle<VkQueryPool, QueryPoolDeleter>(handle, context);
}


inline CommandPool make_command_pool(VkCommandPool handle, const VulkanContext* context) {
    return make_handle<VkCommandPool, CommandPoolDeleter>(handle, context);
}

inline Buffer make_buffer(VkBuffer handle, const VulkanContext* context) {
    return make_handle<VkBuffer, BufferDeleter>(handle, context);
}

inline Image make_image(VkImage handle, const VulkanContext* context) {
    return make_handle<VkImage, ImageDeleter>(handle, context);
}

inline ImageView make_image_view(VkImageView handle, const VulkanContext* context) {
    return make_handle<VkImageView, ImageViewDeleter>(handle, context);
}

inline DeviceMemory make_device_memory(VkDeviceMemory handle, const VulkanContext* context) {
    return make_handle<VkDeviceMemory, DeviceMemoryDeleter>(handle, context);
}

inline Framebuffer make_framebuffer(VkFramebuffer handle, const VulkanContext* context) {
    return make_handle<VkFramebuffer, FramebufferDeleter>(handle, context);
}

// Core context object factory functions
inline Instance make_instance(VkInstance handle, const VulkanContext* context) {
    return make_handle<VkInstance, InstanceDeleter>(handle, context);
}

inline Device make_device(VkDevice handle, const VulkanContext* context) {
    return make_handle<VkDevice, DeviceDeleter>(handle, context);
}

inline SurfaceKHR make_surface_khr(VkSurfaceKHR handle, const VulkanContext* context) {
    return make_handle<VkSurfaceKHR, SurfaceKHRDeleter>(handle, context);
}

inline DebugUtilsMessengerEXT make_debug_utils_messenger_ext(VkDebugUtilsMessengerEXT handle, const VulkanContext* context) {
    return make_handle<VkDebugUtilsMessengerEXT, DebugUtilsMessengerEXTDeleter>(handle, context);
}

// Direct creation factory functions
PipelineCache create_pipeline_cache(const VulkanContext* context, const VkPipelineCacheCreateInfo* createInfo);
Pipeline create_graphics_pipeline(const VulkanContext* context, VkPipelineCache pipelineCache, const VkGraphicsPipelineCreateInfo* createInfo);
Pipeline create_compute_pipeline(const VulkanContext* context, VkPipelineCache pipelineCache, const VkComputePipelineCreateInfo* createInfo);
PipelineLayout create_pipeline_layout(const VulkanContext* context, const VkPipelineLayoutCreateInfo* createInfo);
RenderPass create_render_pass(const VulkanContext* context, const VkRenderPassCreateInfo* createInfo);
DescriptorSetLayout create_descriptor_set_layout(const VulkanContext* context, const VkDescriptorSetLayoutCreateInfo* createInfo);
DescriptorPool create_descriptor_pool(const VulkanContext* context, const VkDescriptorPoolCreateInfo* createInfo);
CommandPool create_command_pool(const VulkanContext* context, const VkCommandPoolCreateInfo* createInfo);
Fence create_fence(const VulkanContext* context, const VkFenceCreateInfo* createInfo);

} // namespace vulkan_raii