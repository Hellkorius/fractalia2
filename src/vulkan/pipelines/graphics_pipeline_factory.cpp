#include "graphics_pipeline_factory.h"
#include "shader_manager.h"
#include <iostream>
#include <chrono>

GraphicsPipelineFactory::GraphicsPipelineFactory(VulkanContext* ctx) : VulkanManagerBase(ctx), layoutBuilder_(ctx) {
}

bool GraphicsPipelineFactory::initialize(ShaderManager* shaderManager, vulkan_raii::PipelineCache* pipelineCache) {
    shaderManager_ = shaderManager;
    pipelineCache_ = pipelineCache;
    return true;
}

std::unique_ptr<CachedGraphicsPipeline> GraphicsPipelineFactory::createPipeline(const GraphicsPipelineState& state) {
    std::cout << "GraphicsPipelineFactory: Starting pipeline creation..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!validatePipelineState(state)) {
        std::cerr << "GraphicsPipelineFactory: Invalid pipeline state" << std::endl;
        return nullptr;
    }
    std::cout << "GraphicsPipelineFactory: Pipeline state validation successful" << std::endl;
    
    auto cachedPipeline = std::make_unique<CachedGraphicsPipeline>();
    cachedPipeline->state = state;
    
    std::cout << "GraphicsPipelineFactory: Creating pipeline layout..." << std::endl;
    VkPipelineLayout rawLayout = layoutBuilder_.createPipelineLayout(state.descriptorSetLayouts, state.pushConstantRanges);
    if (rawLayout == VK_NULL_HANDLE) {
        std::cerr << "GraphicsPipelineFactory: CRITICAL ERROR - Failed to create pipeline layout" << std::endl;
        return nullptr;
    }
    cachedPipeline->layout = vulkan_raii::make_pipeline_layout(rawLayout, context);
    std::cout << "GraphicsPipelineFactory: Pipeline layout created successfully: " << (void*)rawLayout << std::endl;
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(state.vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = state.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(state.vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = state.vertexAttributes.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = state.topology;
    inputAssemblyInfo.primitiveRestartEnable = state.primitiveRestartEnable;
    
    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = state.viewportCount;
    viewportInfo.scissorCount = state.scissorCount;
    
    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = state.depthClampEnable;
    rasterizationInfo.rasterizerDiscardEnable = state.rasterizerDiscardEnable;
    rasterizationInfo.polygonMode = state.polygonMode;
    rasterizationInfo.lineWidth = state.lineWidth;
    rasterizationInfo.cullMode = state.cullMode;
    rasterizationInfo.frontFace = state.frontFace;
    rasterizationInfo.depthBiasEnable = state.depthBiasEnable;
    
    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.sampleShadingEnable = state.sampleShadingEnable;
    multisampleInfo.rasterizationSamples = state.rasterizationSamples;
    multisampleInfo.minSampleShading = state.minSampleShading;
    
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = state.logicOpEnable;
    colorBlendInfo.logicOp = state.logicOp;
    colorBlendInfo.attachmentCount = static_cast<uint32_t>(state.colorBlendAttachments.size());
    colorBlendInfo.pAttachments = state.colorBlendAttachments.data();
    memcpy(colorBlendInfo.blendConstants, state.blendConstants, sizeof(state.blendConstants));
    
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(state.dynamicStates.size());
    dynamicStateInfo.pDynamicStates = state.dynamicStates.data();
    
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = state.depthTestEnable;
    depthStencilInfo.depthWriteEnable = state.depthWriteEnable;
    depthStencilInfo.depthCompareOp = state.depthCompareOp;
    depthStencilInfo.stencilTestEnable = state.stencilTestEnable;
    
    std::cout << "GraphicsPipelineFactory: Loading shaders..." << std::endl;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkShaderModule> shaderModules;
    
    std::cout << "GraphicsPipelineFactory: Shader stages to load: " << state.shaderStages.size() << std::endl;
    for (size_t i = 0; i < state.shaderStages.size(); ++i) {
        std::cout << "  Shader[" << i << "]: " << state.shaderStages[i] << std::endl;
    }
    
    for (const auto& shaderPath : state.shaderStages) {
        std::cout << "GraphicsPipelineFactory: Loading shader: " << shaderPath << std::endl;
        VkShaderModule shaderModule = shaderManager_->loadSPIRVFromFile(shaderPath);
        std::cout << "GraphicsPipelineFactory: Shader module: " << (void*)shaderModule << std::endl;
        
        if (shaderModule == VK_NULL_HANDLE) {
            std::cerr << "GraphicsPipelineFactory: CRITICAL ERROR - Failed to load graphics shader: " << shaderPath << std::endl;
            for (VkShaderModule module : shaderModules) {
                destroyShaderModule(module);
            }
            return nullptr;
        }
        
        shaderModules.push_back(shaderModule);
        
        VkShaderStageFlagBits stage = shaderManager_->getShaderStageFromFilename(shaderPath);
        std::cout << "GraphicsPipelineFactory: Shader stage determined: " << stage << std::endl;
        
        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = stage;
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName = "main";
        
        shaderStages.push_back(shaderStageInfo);
        std::cout << "GraphicsPipelineFactory: Added shader stage successfully" << std::endl;
    }
    
    std::cout << "GraphicsPipelineFactory: All shaders loaded successfully, total stages: " << shaderStages.size() << std::endl;
    
    // Setup dynamic rendering if enabled
    VkPipelineRenderingCreateInfo dynamicRenderingInfo{};
    if (state.useDynamicRendering) {
        dynamicRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        dynamicRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(state.colorAttachmentFormats.size());
        dynamicRenderingInfo.pColorAttachmentFormats = state.colorAttachmentFormats.data();
        dynamicRenderingInfo.depthAttachmentFormat = state.depthAttachmentFormat;
        dynamicRenderingInfo.stencilAttachmentFormat = state.stencilAttachmentFormat;
    }
    
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = state.useDynamicRendering ? &dynamicRenderingInfo : nullptr;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.pDepthStencilState = state.depthTestEnable ? &depthStencilInfo : nullptr;
    pipelineInfo.layout = cachedPipeline->layout.get();
    pipelineInfo.renderPass = state.useDynamicRendering ? VK_NULL_HANDLE : state.renderPass;
    pipelineInfo.subpass = state.useDynamicRendering ? 0 : state.subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    std::cout << "GraphicsPipelineFactory: Creating graphics pipeline with parameters:" << std::endl;
    std::cout << "  Pipeline layout: " << (void*)cachedPipeline->layout << std::endl;
    std::cout << "  Pipeline cache: " << (void*)pipelineCache_ << std::endl;
    if (state.useDynamicRendering) {
        std::cout << "  Dynamic Rendering: ENABLED" << std::endl;
        std::cout << "  Color attachment count: " << state.colorAttachmentFormats.size() << std::endl;
        std::cout << "  Depth format: " << state.depthAttachmentFormat << std::endl;
    } else {
        std::cout << "  Render pass: " << (void*)state.renderPass << std::endl;
        std::cout << "  Subpass: " << state.subpass << std::endl;
    }
    std::cout << "  Shader stages count: " << pipelineInfo.stageCount << std::endl;
    std::cout << "  About to call vkCreateGraphicsPipelines..." << std::endl;
    
    VkPipeline rawPipeline;
    VkResult result = createGraphicsPipelines(pipelineCache_->get(), 1, &pipelineInfo, &rawPipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "GraphicsPipelineFactory: CRITICAL ERROR - Failed to create graphics pipeline!" << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        std::cerr << "  Pipeline layout valid: " << (cachedPipeline->layout ? "YES" : "NO") << std::endl;
        std::cerr << "  Pipeline cache valid: " << (pipelineCache_ ? "YES" : "NO") << std::endl;
        if (state.useDynamicRendering) {
            std::cerr << "  Dynamic Rendering: YES" << std::endl;
        } else {
            std::cerr << "  Render pass valid: " << (state.renderPass != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        }
        std::cerr << "  Device valid: " << (context->getDevice() != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        
        return nullptr;
    } else {
        std::cout << "GraphicsPipelineFactory: Graphics pipeline created successfully!" << std::endl;
    }
    
    cachedPipeline->pipeline = vulkan_raii::make_pipeline(rawPipeline, context);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    cachedPipeline->compilationTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    
    logPipelineCreation(state, cachedPipeline->compilationTime);
    
    return cachedPipeline;
}

bool GraphicsPipelineFactory::validatePipelineState(const GraphicsPipelineState& state) const {
    // For dynamic rendering, render pass must be null; for traditional rendering, it must be valid
    if (state.useDynamicRendering) {
        if (state.renderPass != VK_NULL_HANDLE) {
            std::cerr << "Pipeline state validation failed: render pass must be null for dynamic rendering" << std::endl;
            return false;
        }
        if (state.colorAttachmentFormats.empty()) {
            std::cerr << "Pipeline state validation failed: dynamic rendering requires color attachment formats" << std::endl;
            return false;
        }
    } else {
        if (state.renderPass == VK_NULL_HANDLE) {
            std::cerr << "Pipeline state validation failed: null render pass for traditional rendering" << std::endl;
            return false;
        }
    }
    
    if (state.shaderStages.empty()) {
        std::cerr << "Pipeline state validation failed: no shader stages" << std::endl;
        return false;
    }
    
    return true;
}

void GraphicsPipelineFactory::logPipelineCreation(const GraphicsPipelineState& state, 
                                                 std::chrono::nanoseconds compilationTime) const {
    std::cout << "Created graphics pipeline (compilation time: " 
              << compilationTime.count() / 1000000.0f << "ms)" << std::endl;
}