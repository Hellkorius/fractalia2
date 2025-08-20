#pragma once

#include "graphics_pipeline_manager.h"
#include "compute_pipeline_manager.h"
#include "descriptor_layout_manager.h"
#include "shader_manager.h"
#include "graphics_pipeline_cache.h"
#include "../core/vulkan_context.h"
#include <memory>

// AAA Pipeline System Manager - Unified interface for all pipeline operations
class PipelineSystemManager {
public:
    PipelineSystemManager();
    ~PipelineSystemManager();

    // Initialization
    bool initialize(const VulkanContext& context);
    void cleanup();
    
    // Explicit cleanup before context destruction
    void cleanupBeforeContextDestruction();

    // Manager access
    GraphicsPipelineManager* getGraphicsManager() { return graphicsManager.get(); }
    ComputePipelineManager* getComputeManager() { return computeManager.get(); }
    DescriptorLayoutManager* getLayoutManager() { return layoutManager.get(); }
    ShaderManager* getShaderManager() { return shaderManager.get(); }

    const GraphicsPipelineManager* getGraphicsManager() const { return graphicsManager.get(); }
    const ComputePipelineManager* getComputeManager() const { return computeManager.get(); }
    const DescriptorLayoutManager* getLayoutManager() const { return layoutManager.get(); }
    const ShaderManager* getShaderManager() const { return shaderManager.get(); }

    // High-level pipeline creation (convenience methods)
    struct PipelineCreationInfo {
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        std::string computeShaderPath;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        bool enableMSAA = false;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_2_BIT;
    };

    VkPipeline createGraphicsPipeline(const PipelineCreationInfo& info);
    VkPipeline createComputePipeline(const std::string& computeShaderPath);


    // Integrated operations
    void warmupCommonPipelines();
    void optimizeCaches(uint64_t currentFrame);
    void resetFrameStats();
    
    // Recreate all pipeline caches for swapchain resize operations
    bool recreateAllPipelineCaches();

    // Statistics aggregation
    struct SystemStats {
        PipelineStats graphics;
        ComputePipelineManager::ComputeStats compute;
        DescriptorLayoutManager::LayoutStats layouts;
        ShaderManager::ShaderStats shaders;
    };

    SystemStats getStats() const;

private:
    // Core Vulkan context
    const VulkanContext* context = nullptr;

    // Specialized managers
    std::unique_ptr<ShaderManager> shaderManager;
    std::unique_ptr<DescriptorLayoutManager> layoutManager;
    std::unique_ptr<GraphicsPipelineManager> graphicsManager;
    std::unique_ptr<ComputePipelineManager> computeManager;


    // Internal initialization helpers
    bool initializeManagers();
};