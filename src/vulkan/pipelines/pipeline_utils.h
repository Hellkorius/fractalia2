#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class VulkanFunctionLoader;

// Pipeline-specific utility functions for common pipeline operations
// Eliminates code duplication within the pipelines subsystem
class PipelineUtils {
public:
    PipelineUtils() = delete;
    
    // Pipeline layout utilities
    static VkPipelineLayout createSimplePipelineLayout(VkDevice device,
                                                      const VulkanFunctionLoader& loader,
                                                      const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                                      const std::vector<VkPushConstantRange>& pushConstantRanges = {});
    
    // Render pass utilities
    static VkRenderPass createBasicRenderPass(VkDevice device,
                                             const VulkanFunctionLoader& loader,
                                             VkFormat colorFormat,
                                             VkFormat depthFormat = VK_FORMAT_UNDEFINED,
                                             VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                                             bool enableMSAA = false);
    
    static VkRenderPass createMSAARenderPass(VkDevice device,
                                           const VulkanFunctionLoader& loader,
                                           VkFormat colorFormat,
                                           VkFormat depthFormat,
                                           VkSampleCountFlagBits samples);
    
    // Shader stage utilities
    static VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits stage,
                                                               VkShaderModule module,
                                                               const char* entryPoint = "main",
                                                               const VkSpecializationInfo* specializationInfo = nullptr);
    
    // Vertex input state utilities
    static VkPipelineVertexInputStateCreateInfo createEmptyVertexInputState();
    
    static VkPipelineVertexInputStateCreateInfo createVertexInputState(
        const std::vector<VkVertexInputBindingDescription>& bindings,
        const std::vector<VkVertexInputAttributeDescription>& attributes);
    
    // Input assembly utilities
    static VkPipelineInputAssemblyStateCreateInfo createInputAssemblyState(
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        bool primitiveRestart = false);
    
    // Viewport state utilities
    static VkPipelineViewportStateCreateInfo createViewportState(uint32_t viewportCount = 1,
                                                                uint32_t scissorCount = 1);
    
    // Rasterization state utilities
    static VkPipelineRasterizationStateCreateInfo createRasterizationState(
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        float lineWidth = 1.0f);
    
    // Multisample state utilities
    static VkPipelineMultisampleStateCreateInfo createMultisampleState(
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        bool enableSampleShading = false,
        float minSampleShading = 1.0f);
    
    // Depth stencil state utilities
    static VkPipelineDepthStencilStateCreateInfo createDepthStencilState(
        bool depthTest = true,
        bool depthWrite = true,
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS);
    
    // Color blend state utilities
    static VkPipelineColorBlendStateCreateInfo createColorBlendState(
        const std::vector<VkPipelineColorBlendAttachmentState>& attachments);
    
    static VkPipelineColorBlendAttachmentState createColorBlendAttachment(
        bool enableBlend = false,
        VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    
    // Dynamic state utilities
    static VkPipelineDynamicStateCreateInfo createDynamicState(
        const std::vector<VkDynamicState>& dynamicStates);
    
    // Compute pipeline utilities
    static VkComputePipelineCreateInfo createComputePipelineInfo(
        VkPipelineLayout layout,
        VkPipelineShaderStageCreateInfo shaderStage,
        VkPipeline basePipeline = VK_NULL_HANDLE);
    
    // Pipeline barrier utilities
    static VkMemoryBarrier createMemoryBarrier(VkAccessFlags srcAccess,
                                              VkAccessFlags dstAccess);
    
    static VkBufferMemoryBarrier createBufferBarrier(VkBuffer buffer,
                                                    VkAccessFlags srcAccess,
                                                    VkAccessFlags dstAccess,
                                                    uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                    uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                    VkDeviceSize offset = 0,
                                                    VkDeviceSize size = VK_WHOLE_SIZE);
    
    static VkImageMemoryBarrier createImageBarrier(VkImage image,
                                                  VkAccessFlags srcAccess,
                                                  VkAccessFlags dstAccess,
                                                  VkImageLayout oldLayout,
                                                  VkImageLayout newLayout,
                                                  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                  uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                  uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED);
    
    // Error handling utilities specific to pipelines
    static bool checkPipelineCreation(VkResult result, const char* pipelineType);
    
    // Debug utilities
    static void setDebugName(VkDevice device,
                           const VulkanFunctionLoader& loader,
                           VkPipeline pipeline,
                           const std::string& name);
};