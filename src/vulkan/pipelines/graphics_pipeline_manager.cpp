#include "graphics_pipeline_manager.h"
#include "shader_manager.h"
#include "descriptor_layout_manager.h"
#include "../core/vulkan_constants.h"
#include <iostream>
#include <glm/glm.hpp>

GraphicsPipelineManager::GraphicsPipelineManager(VulkanContext* ctx) 
    : VulkanManagerBase(ctx)
    , maxCacheSize_(DEFAULT_GRAPHICS_CACHE_SIZE)
    , cache_(maxCacheSize_)
    , renderPassManager_(ctx)
    , factory_(ctx)
    , layoutBuilder_(ctx) {
}

GraphicsPipelineManager::~GraphicsPipelineManager() {
    cleanupBeforeContextDestruction();
}

bool GraphicsPipelineManager::initialize(ShaderManager* shaderManager,
                                       DescriptorLayoutManager* layoutManager) {
    shaderManager_ = shaderManager;
    layoutManager_ = layoutManager;
    
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    pipelineCache_ = vulkan_raii::create_pipeline_cache(context, &cacheInfo);
    if (!pipelineCache_) {
        std::cerr << "Failed to create graphics pipeline cache" << std::endl;
        return false;
    }
    
    if (!factory_.initialize(shaderManager, &pipelineCache_)) {
        std::cerr << "Failed to initialize graphics pipeline factory" << std::endl;
        return false;
    }
    
    std::cout << "GraphicsPipelineManager initialized successfully" << std::endl;
    return true;
}

void GraphicsPipelineManager::cleanup() {
    cleanupBeforeContextDestruction();
}

void GraphicsPipelineManager::cleanupBeforeContextDestruction() {
    if (!context) return;
    
    clearCache();
    renderPassManager_.clearCache();
    pipelineCache_.reset();
    
    context = nullptr;
}

VkPipeline GraphicsPipelineManager::getPipeline(const GraphicsPipelineState& state) {
    VkPipeline cachedPipeline = cache_.getPipeline(state);
    if (cachedPipeline != VK_NULL_HANDLE) {
        return cachedPipeline;
    }
    
    auto newPipeline = factory_.createPipeline(state);
    if (!newPipeline) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    VkPipeline pipeline = newPipeline->pipeline.get();
    cache_.storePipeline(state, std::move(newPipeline));
    
    return pipeline;
}

VkPipelineLayout GraphicsPipelineManager::getPipelineLayout(const GraphicsPipelineState& state) {
    VkPipelineLayout cachedLayout = cache_.getPipelineLayout(state);
    if (cachedLayout != VK_NULL_HANDLE) {
        return cachedLayout;
    }
    
    VkPipeline pipeline = getPipeline(state);
    if (pipeline == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    return cache_.getPipelineLayout(state);
}

std::vector<VkPipeline> GraphicsPipelineManager::createPipelinesBatch(const std::vector<GraphicsPipelineState>& states) {
    std::vector<VkPipeline> pipelines;
    pipelines.reserve(states.size());
    
    for (const auto& state : states) {
        VkPipeline pipeline = getPipeline(state);
        pipelines.push_back(pipeline);
    }
    
    return pipelines;
}

VkRenderPass GraphicsPipelineManager::createRenderPass(VkFormat colorFormat, 
                                                      VkFormat depthFormat,
                                                      VkSampleCountFlagBits samples,
                                                      bool enableMSAA) {
    return renderPassManager_.createRenderPass(colorFormat, depthFormat, samples, enableMSAA);
}

GraphicsPipelineState GraphicsPipelineManager::createDefaultState() {
    GraphicsPipelineState state{};
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    state.colorBlendAttachments.push_back(colorBlendAttachment);
    
    return state;
}

GraphicsPipelineState GraphicsPipelineManager::createMSAAState() {
    GraphicsPipelineState state{};
    state.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;
    state.sampleShadingEnable = VK_FALSE;
    state.minSampleShading = 1.0f;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    state.colorBlendAttachments.push_back(colorBlendAttachment);
    
    return state;
}

GraphicsPipelineState GraphicsPipelineManager::createWireframeState() {
    GraphicsPipelineState state = createDefaultState();
    state.polygonMode = VK_POLYGON_MODE_LINE;
    state.lineWidth = 1.0f;
    return state;
}

GraphicsPipelineState GraphicsPipelineManager::createInstancedState() {
    return createDefaultState();
}

void GraphicsPipelineManager::warmupCache(const std::vector<GraphicsPipelineState>& commonStates) {
    for (const auto& state : commonStates) {
        getPipeline(state);
    }
}

void GraphicsPipelineManager::optimizeCache(uint64_t currentFrame) {
    cache_.optimizeCache(currentFrame);
}

void GraphicsPipelineManager::clearCache() {
    cache_.clear();
    renderPassManager_.clearCache();
}

bool GraphicsPipelineManager::recreatePipelineCache() {
    if (!context) {
        std::cerr << "GraphicsPipelineManager: Cannot recreate pipeline cache - no context" << std::endl;
        return false;
    }
    
    if (isRecreating_) {
        std::cerr << "GraphicsPipelineManager: Recreation already in progress, ignoring request" << std::endl;
        return true; // Consider it successful to avoid error cascades
    }
    
    isRecreating_ = true;
    std::cout << "GraphicsPipelineManager: Recreating pipeline cache to prevent corruption" << std::endl;
    
    // Wait for device idle to ensure no pipelines are in use
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    vk.vkDeviceWaitIdle(device);
    
    // Clear caches in dependency order
    clearCache();
    
    if (layoutManager_) {
        std::cout << "GraphicsPipelineManager: Also clearing descriptor layout cache" << std::endl;
        layoutManager_->clearCache();
    }
    
    // Reset pipeline cache
    if (pipelineCache_) {
        std::cout << "GraphicsPipelineManager: Destroying pipeline cache" << std::endl;
        pipelineCache_.reset();
    }
    
    // Create new pipeline cache
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    pipelineCache_ = vulkan_raii::create_pipeline_cache(context, &cacheInfo);
    if (!pipelineCache_) {
        std::cerr << "GraphicsPipelineManager: Failed to recreate pipeline cache" << std::endl;
        isRecreating_ = false;
        return false;
    }
    
    isRecreating_ = false;
    std::cout << "GraphicsPipelineManager: Pipeline cache successfully recreated" << std::endl;
    return true;
}

bool GraphicsPipelineManager::reloadPipeline(const GraphicsPipelineState& state) {
    if (!hotReloadEnabled_) {
        return false;
    }
    
    if (cache_.contains(state)) {
        auto newPipeline = factory_.createPipeline(state);
        if (newPipeline) {
            cache_.storePipeline(state, std::move(newPipeline));
            return true;
        }
    }
    
    return false;
}

namespace GraphicsPipelinePresets {
    GraphicsPipelineState createEntityRenderingState(VkRenderPass renderPass, 
                                                    VkDescriptorSetLayout descriptorLayout) {
        GraphicsPipelineState state{};
        state.renderPass = renderPass;
        state.descriptorSetLayouts.push_back(descriptorLayout);
        
        state.shaderStages = {
            "shaders/vertex.vert.spv",
            "shaders/fragment.frag.spv"
        };
        
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(glm::vec3) + sizeof(glm::vec3);
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertexBindings.push_back(vertexBinding);
        
        // SoA approach: No instance binding for entity data (using storage buffers instead)
        // Only keep vertex position attribute (geometry data)
        
        VkVertexInputAttributeDescription posAttr{};
        posAttr.binding = 0;
        posAttr.location = 0;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = 0;
        state.vertexAttributes.push_back(posAttr);
        
        VkVertexInputAttributeDescription colorAttr{};
        colorAttr.binding = 0;
        colorAttr.location = 1;
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorAttr.offset = sizeof(glm::vec3);
        state.vertexAttributes.push_back(colorAttr);
        
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        state.colorBlendAttachments.push_back(colorBlendAttachment);
        
        // Enable depth testing for proper 3D cube rendering
        state.depthTestEnable = VK_TRUE;
        state.depthWriteEnable = VK_TRUE;
        state.depthCompareOp = VK_COMPARE_OP_LESS;
        state.stencilTestEnable = VK_FALSE;
        
        return state;
    }

    GraphicsPipelineState createParticleRenderingState(VkRenderPass renderPass, 
                                                       VkDescriptorSetLayout descriptorLayout) {
        // Start with entity rendering state and modify for particles
        GraphicsPipelineState state = createEntityRenderingState(renderPass, descriptorLayout);
        
        // Use particle-specific shaders
        state.shaderStages = {
            "shaders/sun_particles.vert.spv",
            "shaders/sun_particles.frag.spv"
        };
        
        // Update vertex input for particle quad (vec2 position)
        state.vertexBindings.clear();
        state.vertexAttributes.clear();
        
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(glm::vec2);  // Only 2D position for quad
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertexBindings.push_back(vertexBinding);
        
        VkVertexInputAttributeDescription posAttr{};
        posAttr.binding = 0;
        posAttr.location = 0;
        posAttr.format = VK_FORMAT_R32G32_SFLOAT;  // vec2
        posAttr.offset = 0;
        state.vertexAttributes.push_back(posAttr);
        
        // Enable alpha blending for particles
        state.colorBlendAttachments.clear();
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        state.colorBlendAttachments.push_back(colorBlendAttachment);
        
        // Disable depth writing for transparent particles
        state.depthWriteEnable = VK_FALSE;
        
        return state;
    }

    GraphicsPipelineState createSunSystemRenderingState(VkRenderPass renderPass, 
                                                        VkDescriptorSetLayout descriptorLayout) {
        // Create a clean state from scratch to avoid inheritance issues
        GraphicsPipelineState state{};
        state.renderPass = renderPass;
        state.descriptorSetLayouts.push_back(descriptorLayout);
        
        // Use sun system specific shaders
        state.shaderStages = {
            "shaders/sun_system.vert.spv",
            "shaders/sun_system.frag.spv"
        };
        
        // Vertex input for simple 2D quad (vec2 position)
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(glm::vec2);  // Only 2D position for quad
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertexBindings.push_back(vertexBinding);
        
        VkVertexInputAttributeDescription posAttr{};
        posAttr.binding = 0;
        posAttr.location = 0;
        posAttr.format = VK_FORMAT_R32G32_SFLOAT;  // vec2
        posAttr.offset = 0;
        state.vertexAttributes.push_back(posAttr);
        
        // Input assembly
        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        state.primitiveRestartEnable = VK_FALSE;
        
        // Rasterization
        state.depthClampEnable = VK_FALSE;
        state.rasterizerDiscardEnable = VK_FALSE;
        state.polygonMode = VK_POLYGON_MODE_FILL;
        state.lineWidth = 1.0f;
        state.cullMode = VK_CULL_MODE_NONE;  // Billboard quads, no culling
        state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        state.depthBiasEnable = VK_FALSE;
        
        // Multisampling
        state.sampleShadingEnable = VK_FALSE;
        state.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;  // Match render pass
        state.minSampleShading = 1.0f;
        
        // Depth/Stencil
        state.depthTestEnable = VK_TRUE;
        state.depthWriteEnable = VK_FALSE;  // Don't write depth for transparent particles
        state.depthCompareOp = VK_COMPARE_OP_LESS;
        state.stencilTestEnable = VK_FALSE;
        
        // Color blending - additive blending for light effect
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE; // Additive blending
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        state.colorBlendAttachments.push_back(colorBlendAttachment);
        
        state.logicOpEnable = VK_FALSE;
        state.logicOp = VK_LOGIC_OP_COPY;
        
        // Push constants for render mode (0=sun disc, 1=particles)
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(int) * 2;  // renderMode, instanceId
        state.pushConstantRanges.push_back(pushConstant);
        
        return state;
    }
}