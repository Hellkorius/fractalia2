#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <chrono>
#include "../core/vulkan_context.h"

#include "../core/vulkan_raii.h"
#include "graphics_pipeline_state_hash.h"
#include "graphics_pipeline_cache.h"
#include "graphics_pipeline_layout_builder.h"

class ShaderManager;

class GraphicsPipelineFactory {
public:
    explicit GraphicsPipelineFactory(VulkanContext* ctx);
    ~GraphicsPipelineFactory() = default;

    bool initialize(ShaderManager* shaderManager, vulkan_raii::PipelineCache* pipelineCache);
    
    std::unique_ptr<CachedGraphicsPipeline> createPipeline(const GraphicsPipelineState& state);

private:
    VulkanContext* context;
    ShaderManager* shaderManager_ = nullptr;
    vulkan_raii::PipelineCache* pipelineCache_ = nullptr;
    GraphicsPipelineLayoutBuilder layoutBuilder_;
    
    bool validatePipelineState(const GraphicsPipelineState& state) const;
    void logPipelineCreation(const GraphicsPipelineState& state, 
                           std::chrono::nanoseconds compilationTime) const;
};