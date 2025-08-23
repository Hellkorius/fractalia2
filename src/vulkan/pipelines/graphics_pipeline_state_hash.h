#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct GraphicsPipelineState {
    std::vector<std::string> shaderStages;
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 primitiveRestartEnable = VK_FALSE;
    
    uint32_t viewportCount = 1;
    uint32_t scissorCount = 1;
    
    VkBool32 depthClampEnable = VK_FALSE;
    VkBool32 rasterizerDiscardEnable = VK_FALSE;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkBool32 depthBiasEnable = VK_FALSE;
    float lineWidth = 1.0f;
    
    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;
    VkBool32 sampleShadingEnable = VK_FALSE;
    float minSampleShading = 1.0f;
    
    VkBool32 depthTestEnable = VK_FALSE;
    VkBool32 depthWriteEnable = VK_FALSE;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkBool32 stencilTestEnable = VK_FALSE;
    
    VkBool32 logicOpEnable = VK_FALSE;
    VkLogicOp logicOp = VK_LOGIC_OP_COPY;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
    
    // Vulkan 1.3 Dynamic Rendering support
    bool useDynamicRendering = false;
    std::vector<VkFormat> colorAttachmentFormats;
    VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    
    bool operator==(const GraphicsPipelineState& other) const;
    size_t getHash() const;
};

class GraphicsPipelineStateHash {
public:
    std::size_t operator()(const GraphicsPipelineState& state) const {
        return state.getHash();
    }
    
    static bool compareStates(const GraphicsPipelineState& a, const GraphicsPipelineState& b);
    static size_t hashPushConstantRanges(const std::vector<VkPushConstantRange>& ranges);
    static size_t hashVertexBindings(const std::vector<VkVertexInputBindingDescription>& bindings);
    static size_t hashVertexAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes);
    static size_t hashColorBlendAttachments(const std::vector<VkPipelineColorBlendAttachmentState>& attachments);
};