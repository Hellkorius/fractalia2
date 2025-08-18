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

} // namespace vulkan_raii