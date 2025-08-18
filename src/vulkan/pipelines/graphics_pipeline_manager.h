#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include "../core/vulkan_context.h"
#include "../core/vulkan_manager_base.h"
#include "../core/vulkan_raii.h"

// Forward declarations
class ShaderManager;
class DescriptorLayoutManager;

// Graphics Pipeline State Object (PSO) for caching
struct GraphicsPipelineState {
    // Shader stages
    std::vector<std::string> shaderStages;  // Paths to shader files
    
    // Vertex input state
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    
    // Input assembly state
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 primitiveRestartEnable = VK_FALSE;
    
    // Viewport state (dynamic by default for AAA flexibility)
    uint32_t viewportCount = 1;
    uint32_t scissorCount = 1;
    
    // Rasterization state
    VkBool32 depthClampEnable = VK_FALSE;
    VkBool32 rasterizerDiscardEnable = VK_FALSE;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkBool32 depthBiasEnable = VK_FALSE;
    float lineWidth = 1.0f;
    
    // Multisample state
    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;
    VkBool32 sampleShadingEnable = VK_FALSE;
    float minSampleShading = 1.0f;
    
    // Depth stencil state
    VkBool32 depthTestEnable = VK_FALSE;
    VkBool32 depthWriteEnable = VK_FALSE;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkBool32 stencilTestEnable = VK_FALSE;
    
    // Color blend state
    VkBool32 logicOpEnable = VK_FALSE;
    VkLogicOp logicOp = VK_LOGIC_OP_COPY;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    // Render pass compatibility
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
    
    // Descriptor set layouts
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    
    // Push constant ranges
    std::vector<VkPushConstantRange> pushConstantRanges;
    
    // Comparison for caching
    bool operator==(const GraphicsPipelineState& other) const;
    
    // Hash function for unordered_map
    size_t getHash() const;
};

// Hash specialization for PSO caching
struct GraphicsPipelineStateHash {
    std::size_t operator()(const GraphicsPipelineState& state) const {
        return state.getHash();
    }
};

// Cached pipeline object with metadata
struct CachedGraphicsPipeline {
    vulkan_raii::Pipeline pipeline;
    vulkan_raii::PipelineLayout layout;
    GraphicsPipelineState state;
    uint64_t lastUsedFrame = 0;
    uint32_t useCount = 0;
    
    // Performance metrics
    std::chrono::nanoseconds compilationTime{0};
    bool isHotPath = false;  // Frequently used pipelines
};

// AAA Graphics Pipeline Manager with advanced caching
class GraphicsPipelineManager : public VulkanManagerBase {
public:
    explicit GraphicsPipelineManager(VulkanContext* ctx);
    ~GraphicsPipelineManager();

    // Initialization
    bool initialize(ShaderManager* shaderManager,
                   DescriptorLayoutManager* layoutManager);
    void cleanup();
    void cleanupBeforeContextDestruction();

    // Pipeline creation and caching
    VkPipeline getPipeline(const GraphicsPipelineState& state);
    VkPipelineLayout getPipelineLayout(const GraphicsPipelineState& state);
    
    // Batch pipeline creation for reduced driver overhead
    std::vector<VkPipeline> createPipelinesBatch(const std::vector<GraphicsPipelineState>& states);
    
    // Render pass management
    VkRenderPass createRenderPass(VkFormat colorFormat, 
                                 VkFormat depthFormat = VK_FORMAT_UNDEFINED,
                                 VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                                 bool enableMSAA = false);
    
    // Default PSO presets for common use cases
    GraphicsPipelineState createDefaultState();
    GraphicsPipelineState createMSAAState();
    GraphicsPipelineState createWireframeState();
    GraphicsPipelineState createInstancedState();
    
    // Cache management
    void warmupCache(const std::vector<GraphicsPipelineState>& commonStates);
    void optimizeCache(uint64_t currentFrame);  // Remove unused pipelines
    void clearCache();
    
    // CRITICAL FIX: Pipeline cache corruption fix for second resize crash
    bool recreatePipelineCache();
    
    // Access to layout manager for descriptor layout creation
    DescriptorLayoutManager* getLayoutManager() { return layoutManager; }
    const DescriptorLayoutManager* getLayoutManager() const { return layoutManager; }
    
    // Statistics and debugging
    struct PipelineStats {
        uint32_t totalPipelines = 0;
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        uint32_t compilationsThisFrame = 0;
        std::chrono::nanoseconds totalCompilationTime{0};
        float hitRatio = 0.0f;
    };
    
    PipelineStats getStats() const { return stats; }
    void resetFrameStats();
    void debugPrintCache() const;
    
    // Hot reloading support for development
    bool reloadPipeline(const GraphicsPipelineState& state);
    void enableHotReload(bool enable) { hotReloadEnabled = enable; }

private:
    // Core Vulkan objects
    vulkan_raii::PipelineCache pipelineCache;
    
    // Dependencies
    ShaderManager* shaderManager = nullptr;
    DescriptorLayoutManager* layoutManager = nullptr;
    
    // Pipeline cache
    std::unordered_map<GraphicsPipelineState, std::unique_ptr<CachedGraphicsPipeline>, GraphicsPipelineStateHash> pipelineCache_;
    
    // Render pass cache
    std::unordered_map<size_t, vulkan_raii::RenderPass> renderPassCache;
    
    // Statistics
    mutable PipelineStats stats;
    
    // Configuration
    bool hotReloadEnabled = false;
    uint32_t maxCacheSize = 1024;
    uint64_t cacheCleanupInterval = 1000;  // frames
    
    // Internal pipeline creation
    std::unique_ptr<CachedGraphicsPipeline> createPipelineInternal(const GraphicsPipelineState& state);
    VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                         const std::vector<VkPushConstantRange>& pushConstants);
    
    // Cache management helpers
    void evictLeastRecentlyUsed();
    bool shouldEvictPipeline(const CachedGraphicsPipeline& pipeline, uint64_t currentFrame) const;
    
    // Validation and error handling
    bool validatePipelineState(const GraphicsPipelineState& state) const;
    void logPipelineCreation(const GraphicsPipelineState& state, 
                           std::chrono::nanoseconds compilationTime) const;
    
    // Hash generation helpers
    size_t hashVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings,
                          const std::vector<VkVertexInputAttributeDescription>& attributes) const;
    size_t hashColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments) const;
};

// Utility functions for common pipeline configurations
namespace GraphicsPipelinePresets {
    // Entity rendering pipeline (for your use case)
    GraphicsPipelineState createEntityRenderingState(VkRenderPass renderPass, 
                                                    VkDescriptorSetLayout descriptorLayout);
    
    // Debug wireframe overlay
    GraphicsPipelineState createWireframeOverlayState(VkRenderPass renderPass);
    
    // UI/Overlay rendering
    GraphicsPipelineState createUIRenderingState(VkRenderPass renderPass);
    
    // Shadow mapping
    GraphicsPipelineState createShadowMappingState(VkRenderPass renderPass);
}