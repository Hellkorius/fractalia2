#include "graphics_pipeline_manager.h"
#include "shader_manager.h"
#include "descriptor_layout_manager.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cassert>
#include <glm/glm.hpp>

// GraphicsPipelineState implementation
bool GraphicsPipelineState::operator==(const GraphicsPipelineState& other) const {
    // Manual comparison for pushConstantRanges since VkPushConstantRange doesn't have operator==
    if (pushConstantRanges.size() != other.pushConstantRanges.size()) {
        return false;
    }
    for (size_t i = 0; i < pushConstantRanges.size(); ++i) {
        const auto& a = pushConstantRanges[i];
        const auto& b = other.pushConstantRanges[i];
        if (a.stageFlags != b.stageFlags || a.offset != b.offset || a.size != b.size) {
            return false;
        }
    }
    
    // Manual comparison for vertexBindings since VkVertexInputBindingDescription doesn't have operator==
    if (vertexBindings.size() != other.vertexBindings.size()) {
        return false;
    }
    for (size_t i = 0; i < vertexBindings.size(); ++i) {
        const auto& a = vertexBindings[i];
        const auto& b = other.vertexBindings[i];
        if (a.binding != b.binding || a.stride != b.stride || a.inputRate != b.inputRate) {
            return false;
        }
    }
    
    // Manual comparison for vertexAttributes since VkVertexInputAttributeDescription doesn't have operator==
    if (vertexAttributes.size() != other.vertexAttributes.size()) {
        return false;
    }
    for (size_t i = 0; i < vertexAttributes.size(); ++i) {
        const auto& a = vertexAttributes[i];
        const auto& b = other.vertexAttributes[i];
        if (a.location != b.location || a.binding != b.binding || a.format != b.format || a.offset != b.offset) {
            return false;
        }
    }
    
    // Manual comparison for colorBlendAttachments since VkPipelineColorBlendAttachmentState doesn't have operator==
    if (colorBlendAttachments.size() != other.colorBlendAttachments.size()) {
        return false;
    }
    for (size_t i = 0; i < colorBlendAttachments.size(); ++i) {
        const auto& a = colorBlendAttachments[i];
        const auto& b = other.colorBlendAttachments[i];
        if (a.colorWriteMask != b.colorWriteMask || a.blendEnable != b.blendEnable ||
            a.srcColorBlendFactor != b.srcColorBlendFactor || a.dstColorBlendFactor != b.dstColorBlendFactor ||
            a.colorBlendOp != b.colorBlendOp || a.srcAlphaBlendFactor != b.srcAlphaBlendFactor ||
            a.dstAlphaBlendFactor != b.dstAlphaBlendFactor || a.alphaBlendOp != b.alphaBlendOp) {
            return false;
        }
    }
    
    return shaderStages == other.shaderStages &&
           topology == other.topology &&
           primitiveRestartEnable == other.primitiveRestartEnable &&
           polygonMode == other.polygonMode &&
           cullMode == other.cullMode &&
           frontFace == other.frontFace &&
           rasterizationSamples == other.rasterizationSamples &&
           renderPass == other.renderPass &&
           subpass == other.subpass &&
           descriptorSetLayouts == other.descriptorSetLayouts;
}

size_t GraphicsPipelineState::getHash() const {
    size_t hash = 0;
    
    // Hash shader stages
    for (const auto& stage : shaderStages) {
        hash ^= std::hash<std::string>{}(stage) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash vertex input
    for (const auto& binding : vertexBindings) {
        hash ^= std::hash<uint32_t>{}(binding.binding) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(binding.stride) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(binding.inputRate) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    for (const auto& attr : vertexAttributes) {
        hash ^= std::hash<uint32_t>{}(attr.location) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(attr.binding) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(attr.format) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(attr.offset) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash render state
    hash ^= std::hash<uint32_t>{}(topology) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(polygonMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(cullMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(rasterizationSamples) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // Hash render pass
    hash ^= std::hash<VkRenderPass>{}(renderPass) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(subpass) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    return hash;
}

// GraphicsPipelineManager implementation
GraphicsPipelineManager::GraphicsPipelineManager(VulkanContext* ctx) : VulkanManagerBase(ctx) {
}

GraphicsPipelineManager::~GraphicsPipelineManager() {
    cleanup();
}

bool GraphicsPipelineManager::initialize(ShaderManager* shaderManager,
                                       DescriptorLayoutManager* layoutManager) {
    this->shaderManager = shaderManager;
    this->layoutManager = layoutManager;
    
    // Create pipeline cache for optimal performance
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    VkResult result = createPipelineCache(&cacheInfo, &pipelineCache);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline cache: " << result << std::endl;
        return false;
    }
    
    std::cout << "GraphicsPipelineManager initialized successfully" << std::endl;
    return true;
}

void GraphicsPipelineManager::cleanup() {
    if (!context) return;
    
    // Clear pipeline cache
    clearCache();
    
    // Destroy render passes
    for (auto& [hash, renderPass] : renderPassCache) {
        destroyRenderPass(renderPass);
    }
    renderPassCache.clear();
    
    // Destroy pipeline cache
    if (pipelineCache != VK_NULL_HANDLE) {
        destroyPipelineCache(pipelineCache);
        pipelineCache = VK_NULL_HANDLE;
    }
    
    context = nullptr;
}

VkPipeline GraphicsPipelineManager::getPipeline(const GraphicsPipelineState& state) {
    // Check cache first
    auto it = pipelineCache_.find(state);
    if (it != pipelineCache_.end()) {
        // Cache hit
        stats.cacheHits++;
        it->second->lastUsedFrame = stats.cacheHits + stats.cacheMisses;  // Rough frame counter
        it->second->useCount++;
        return it->second->pipeline;
    }
    
    // Cache miss - create new pipeline
    stats.cacheMisses++;
    stats.compilationsThisFrame++;
    
    auto cachedPipeline = createPipelineInternal(state);
    if (!cachedPipeline) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    VkPipeline pipeline = cachedPipeline->pipeline;
    
    // Store in cache
    pipelineCache_[state] = std::move(cachedPipeline);
    stats.totalPipelines++;
    
    // Check if cache needs cleanup
    if (pipelineCache_.size() > maxCacheSize) {
        evictLeastRecentlyUsed();
    }
    
    return pipeline;
}

VkPipelineLayout GraphicsPipelineManager::getPipelineLayout(const GraphicsPipelineState& state) {
    auto it = pipelineCache_.find(state);
    if (it != pipelineCache_.end()) {
        return it->second->layout;
    }
    
    // If pipeline doesn't exist, create it
    VkPipeline pipeline = getPipeline(state);
    if (pipeline == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    // Should now be in cache
    it = pipelineCache_.find(state);
    assert(it != pipelineCache_.end());
    return it->second->layout;
}

std::unique_ptr<CachedGraphicsPipeline> GraphicsPipelineManager::createPipelineInternal(const GraphicsPipelineState& state) {
    std::cout << "GraphicsPipelineManager: Starting createPipelineInternal..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!validatePipelineState(state)) {
        std::cerr << "GraphicsPipelineManager: Invalid pipeline state" << std::endl;
        return nullptr;
    }
    std::cout << "GraphicsPipelineManager: Pipeline state validation successful" << std::endl;
    
    auto cachedPipeline = std::make_unique<CachedGraphicsPipeline>();
    cachedPipeline->state = state;
    
    // Create pipeline layout
    std::cout << "GraphicsPipelineManager: Creating pipeline layout..." << std::endl;
    cachedPipeline->layout = createPipelineLayout(state.descriptorSetLayouts, state.pushConstantRanges);
    if (cachedPipeline->layout == VK_NULL_HANDLE) {
        std::cerr << "GraphicsPipelineManager: CRITICAL ERROR - Failed to create pipeline layout" << std::endl;
        return nullptr;
    }
    std::cout << "GraphicsPipelineManager: Pipeline layout created successfully: " << (void*)cachedPipeline->layout << std::endl;
    
    // Shader loading is implemented later in this function using ShaderManager
    
    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(state.vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = state.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(state.vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = state.vertexAttributes.data();
    
    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = state.topology;
    inputAssemblyInfo.primitiveRestartEnable = state.primitiveRestartEnable;
    
    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = state.viewportCount;
    viewportInfo.scissorCount = state.scissorCount;
    
    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = state.depthClampEnable;
    rasterizationInfo.rasterizerDiscardEnable = state.rasterizerDiscardEnable;
    rasterizationInfo.polygonMode = state.polygonMode;
    rasterizationInfo.lineWidth = state.lineWidth;
    rasterizationInfo.cullMode = state.cullMode;
    rasterizationInfo.frontFace = state.frontFace;
    rasterizationInfo.depthBiasEnable = state.depthBiasEnable;
    
    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.sampleShadingEnable = state.sampleShadingEnable;
    multisampleInfo.rasterizationSamples = state.rasterizationSamples;
    multisampleInfo.minSampleShading = state.minSampleShading;
    
    // Color blend state
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = state.logicOpEnable;
    colorBlendInfo.logicOp = state.logicOp;
    colorBlendInfo.attachmentCount = static_cast<uint32_t>(state.colorBlendAttachments.size());
    colorBlendInfo.pAttachments = state.colorBlendAttachments.data();
    memcpy(colorBlendInfo.blendConstants, state.blendConstants, sizeof(state.blendConstants));
    
    // Dynamic state
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(state.dynamicStates.size());
    dynamicStateInfo.pDynamicStates = state.dynamicStates.data();
    
    // Depth stencil state (if needed)
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = state.depthTestEnable;
    depthStencilInfo.depthWriteEnable = state.depthWriteEnable;
    depthStencilInfo.depthCompareOp = state.depthCompareOp;
    depthStencilInfo.stencilTestEnable = state.stencilTestEnable;
    
    // Load shaders through ShaderManager
    std::cout << "GraphicsPipelineManager: Loading shaders..." << std::endl;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkShaderModule> shaderModules;  // Keep track for cleanup
    
    std::cout << "GraphicsPipelineManager: Shader stages to load: " << state.shaderStages.size() << std::endl;
    for (size_t i = 0; i < state.shaderStages.size(); ++i) {
        std::cout << "  Shader[" << i << "]: " << state.shaderStages[i] << std::endl;
    }
    
    for (const auto& shaderPath : state.shaderStages) {
        std::cout << "GraphicsPipelineManager: Loading shader: " << shaderPath << std::endl;
        VkShaderModule shaderModule = shaderManager->loadSPIRVFromFile(shaderPath);
        std::cout << "GraphicsPipelineManager: Shader module: " << (void*)shaderModule << std::endl;
        
        if (shaderModule == VK_NULL_HANDLE) {
            std::cerr << "GraphicsPipelineManager: CRITICAL ERROR - Failed to load graphics shader: " << shaderPath << std::endl;
            // Cleanup previously loaded shaders
            for (VkShaderModule module : shaderModules) {
                destroyShaderModule(module);
            }
            if (cachedPipeline->layout != VK_NULL_HANDLE) {
                destroyPipelineLayout(cachedPipeline->layout);
            }
            return nullptr;
        }
        
        shaderModules.push_back(shaderModule);
        
        // Determine shader stage from filename
        VkShaderStageFlagBits stage = shaderManager->getShaderStageFromFilename(shaderPath);
        std::cout << "GraphicsPipelineManager: Shader stage determined: " << stage << std::endl;
        
        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = stage;
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName = "main";
        
        shaderStages.push_back(shaderStageInfo);
        std::cout << "GraphicsPipelineManager: Added shader stage successfully" << std::endl;
    }
    
    std::cout << "GraphicsPipelineManager: All shaders loaded successfully, total stages: " << shaderStages.size() << std::endl;
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
    pipelineInfo.layout = cachedPipeline->layout;
    pipelineInfo.renderPass = state.renderPass;
    pipelineInfo.subpass = state.subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    // Create graphics pipeline with detailed error logging
    std::cout << "GraphicsPipelineManager: Creating graphics pipeline with parameters:" << std::endl;
    std::cout << "  Pipeline layout: " << (void*)cachedPipeline->layout << std::endl;
    std::cout << "  Pipeline cache: " << (void*)pipelineCache << std::endl;
    std::cout << "  Render pass: " << (void*)state.renderPass << std::endl;
    std::cout << "  Subpass: " << state.subpass << std::endl;
    std::cout << "  Shader stages count: " << pipelineInfo.stageCount << std::endl;
    std::cout << "  About to call vkCreateGraphicsPipelines..." << std::endl;
    
    VkResult result = createGraphicsPipelines(pipelineCache, 1, &pipelineInfo, &cachedPipeline->pipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "GraphicsPipelineManager: CRITICAL ERROR - Failed to create graphics pipeline!" << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        std::cerr << "  Pipeline layout valid: " << (cachedPipeline->layout != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        std::cerr << "  Pipeline cache valid: " << (pipelineCache != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        std::cerr << "  Render pass valid: " << (state.renderPass != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        std::cerr << "  Device valid: " << (context->getDevice() != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        
        if (cachedPipeline->layout != VK_NULL_HANDLE) {
            destroyPipelineLayout(cachedPipeline->layout);
        }
        return nullptr;
    } else {
        std::cout << "GraphicsPipelineManager: Graphics pipeline created successfully!" << std::endl;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    cachedPipeline->compilationTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    stats.totalCompilationTime += cachedPipeline->compilationTime;
    
    logPipelineCreation(state, cachedPipeline->compilationTime);
    
    return cachedPipeline;
}

VkPipelineLayout GraphicsPipelineManager::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                                              const std::vector<VkPushConstantRange>& pushConstants) {
    std::cout << "GraphicsPipelineManager: Creating graphics pipeline layout" << std::endl;
    std::cout << "  Descriptor set layouts count: " << setLayouts.size() << std::endl;
    for (size_t i = 0; i < setLayouts.size(); ++i) {
        std::cout << "    Layout[" << i << "]: " << (void*)setLayouts[i] << (setLayouts[i] != VK_NULL_HANDLE ? " (valid)" : " (INVALID!)") << std::endl;
    }
    std::cout << "  Push constant ranges count: " << pushConstants.size() << std::endl;
    
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
    layoutInfo.pPushConstantRanges = pushConstants.data();
    
    VkPipelineLayout layout;
    VkResult result = loader->vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
    
    if (result != VK_SUCCESS) {
        std::cerr << "GraphicsPipelineManager: CRITICAL ERROR - Failed to create graphics pipeline layout!" << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        std::cerr << "  Device valid: " << (context->getDevice() != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        return VK_NULL_HANDLE;
    } else {
        std::cout << "GraphicsPipelineManager: Graphics pipeline layout created successfully: " << (void*)layout << std::endl;
    }
    
    return layout;
}

VkRenderPass GraphicsPipelineManager::createRenderPass(VkFormat colorFormat, 
                                                      VkFormat depthFormat,
                                                      VkSampleCountFlagBits samples,
                                                      bool enableMSAA) {
    // Create hash for render pass configuration
    size_t hash = 0;
    hash ^= std::hash<uint32_t>{}(colorFormat) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(depthFormat) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(samples) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<bool>{}(enableMSAA) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // Check cache
    auto it = renderPassCache.find(hash);
    if (it != renderPassCache.end()) {
        return it->second;
    }
    
    // Create new render pass
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    VkAttachmentReference depthAttachmentRef{};
    VkAttachmentReference resolveAttachmentRef{};
    
    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = enableMSAA ? samples : VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = enableMSAA ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = enableMSAA ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments.push_back(colorAttachment);
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRefs.push_back(colorAttachmentRef);
    
    // MSAA resolve attachment
    if (enableMSAA) {
        VkAttachmentDescription resolveAttachment{};
        resolveAttachment.format = colorFormat;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(resolveAttachment);
        
        resolveAttachmentRef.attachment = 1;
        resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    
    // Depth attachment (if specified)
    bool hasDepth = (depthFormat != VK_FORMAT_UNDEFINED);
    if (hasDepth) {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = enableMSAA ? samples : VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachment);
        
        depthAttachmentRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    
    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = hasDepth ? &depthAttachmentRef : nullptr;
    subpass.pResolveAttachments = enableMSAA ? &resolveAttachmentRef : nullptr;
    
    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    if (hasDepth) {
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    // Create render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    VkRenderPass renderPass;
    VkResult result = loader->vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create render pass: " << result << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Cache the render pass
    renderPassCache[hash] = renderPass;
    
    return renderPass;
}

void GraphicsPipelineManager::clearCache() {
    if (!context) return;
    
    // Destroy all cached pipelines
    for (auto& [state, pipeline] : pipelineCache_) {
        if (pipeline->pipeline != VK_NULL_HANDLE) {
            destroyPipeline(pipeline->pipeline);
        }
        if (pipeline->layout != VK_NULL_HANDLE) {
            destroyPipelineLayout(pipeline->layout);
        }
    }
    
    // CRITICAL FIX FOR SECOND RESIZE CRASH: Clear render pass cache to prevent stale render passes
    // Render passes become invalid after swapchain recreation and must be recreated
    std::cout << "GraphicsPipelineManager: Clearing render pass cache (" << renderPassCache.size() << " render passes)" << std::endl;
    for (auto& [hash, renderPass] : renderPassCache) {
        if (renderPass != VK_NULL_HANDLE) {
            destroyRenderPass(renderPass);
        }
    }
    renderPassCache.clear();
    std::cout << "GraphicsPipelineManager: Render pass cache cleared successfully" << std::endl;
    
    pipelineCache_.clear();
    stats.totalPipelines = 0;
}

bool GraphicsPipelineManager::recreatePipelineCache() {
    if (!context) {
        std::cerr << "GraphicsPipelineManager: Cannot recreate pipeline cache - no context" << std::endl;
        return false;
    }
    
    std::cout << "GraphicsPipelineManager: CRITICAL FIX - Recreating pipeline cache to prevent second resize corruption" << std::endl;
    
    // Clear existing pipeline objects first
    clearCache();
    
    // CRITICAL FIX: Also clear descriptor layout cache to prevent stale layout handles
    // Descriptor layouts may become invalid after command pool recreation
    if (layoutManager) {
        std::cout << "GraphicsPipelineManager: Also clearing descriptor layout cache to prevent stale handles" << std::endl;
        layoutManager->clearCache();
    }
    
    // Destroy and recreate the VkPipelineCache object itself
    if (pipelineCache != VK_NULL_HANDLE) {
        std::cout << "GraphicsPipelineManager: Destroying corrupted pipeline cache" << std::endl;
        destroyPipelineCache(pipelineCache);
        pipelineCache = VK_NULL_HANDLE;
    }
    
    // Create fresh pipeline cache
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    if (createPipelineCache(&cacheInfo, &pipelineCache) != VK_SUCCESS) {
        std::cerr << "GraphicsPipelineManager: CRITICAL FAILURE - Failed to recreate pipeline cache" << std::endl;
        return false;
    }
    
    std::cout << "GraphicsPipelineManager: Pipeline cache successfully recreated" << std::endl;
    return true;
}

void GraphicsPipelineManager::evictLeastRecentlyUsed() {
    if (pipelineCache_.empty()) return;
    
    // Find least recently used pipeline
    auto lruIt = pipelineCache_.begin();
    for (auto it = pipelineCache_.begin(); it != pipelineCache_.end(); ++it) {
        if (it->second->lastUsedFrame < lruIt->second->lastUsedFrame) {
            lruIt = it;
        }
    }
    
    // Destroy the pipeline
    if (lruIt->second->pipeline != VK_NULL_HANDLE) {
        destroyPipeline(lruIt->second->pipeline);
    }
    if (lruIt->second->layout != VK_NULL_HANDLE) {
        destroyPipelineLayout(lruIt->second->layout);
    }
    
    pipelineCache_.erase(lruIt);
    stats.totalPipelines--;
}

bool GraphicsPipelineManager::validatePipelineState(const GraphicsPipelineState& state) const {
    // Basic validation
    if (state.renderPass == VK_NULL_HANDLE) {
        std::cerr << "Pipeline state validation failed: null render pass" << std::endl;
        return false;
    }
    
    if (state.shaderStages.empty()) {
        std::cerr << "Pipeline state validation failed: no shader stages" << std::endl;
        return false;
    }
    
    return true;
}

void GraphicsPipelineManager::logPipelineCreation(const GraphicsPipelineState& state, 
                                                 std::chrono::nanoseconds compilationTime) const {
    std::cout << "Created graphics pipeline (compilation time: " 
              << compilationTime.count() / 1000000.0f << "ms)" << std::endl;
}

void GraphicsPipelineManager::resetFrameStats() {
    stats.compilationsThisFrame = 0;
    stats.hitRatio = static_cast<float>(stats.cacheHits) / static_cast<float>(stats.cacheHits + stats.cacheMisses);
}

GraphicsPipelineState GraphicsPipelineManager::createDefaultState() {
    GraphicsPipelineState state{};
    
    // Default color blend attachment
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    state.colorBlendAttachments.push_back(colorBlendAttachment);
    
    return state;
}

// GraphicsPipelinePresets namespace implementation
namespace GraphicsPipelinePresets {
    GraphicsPipelineState createEntityRenderingState(VkRenderPass renderPass, 
                                                    VkDescriptorSetLayout descriptorLayout) {
        GraphicsPipelineState state{};
        state.renderPass = renderPass;
        state.descriptorSetLayouts.push_back(descriptorLayout);
        
        // Add shader stages for entity rendering
        state.shaderStages = {
            "shaders/vertex.vert.spv",
            "shaders/fragment.frag.spv"
        };
        
        // Entity-specific vertex input - vertex geometry + instance data
        // Binding 0: Vertex geometry (position + color)
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(glm::vec3) + sizeof(glm::vec3);  // position + color (Vertex struct)
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertexBindings.push_back(vertexBinding);
        
        // Binding 1: GPUEntity instance data
        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding = 1;
        instanceBinding.stride = 128;  // sizeof(GPUEntity) = 128 bytes
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        state.vertexBindings.push_back(instanceBinding);
        
        // Vertex attributes
        // Location 0: Position (binding 0)
        VkVertexInputAttributeDescription posAttr{};
        posAttr.binding = 0;
        posAttr.location = 0;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = 0;
        state.vertexAttributes.push_back(posAttr);
        
        // Location 1: Color (binding 0) - from Vertex struct
        VkVertexInputAttributeDescription colorAttr{};
        colorAttr.binding = 0;
        colorAttr.location = 1;
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorAttr.offset = sizeof(glm::vec3);
        state.vertexAttributes.push_back(colorAttr);
        
        // Location 7: movementParams0 (amplitude, frequency, phase, timeOffset)
        VkVertexInputAttributeDescription moveParams0Attr{};
        moveParams0Attr.binding = 1;
        moveParams0Attr.location = 7;
        moveParams0Attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        moveParams0Attr.offset = 80;  // offsetof(GPUEntity, movementParams0)
        state.vertexAttributes.push_back(moveParams0Attr);
        
        // Location 8: movementParams1 (center.xyz, movementType)
        VkVertexInputAttributeDescription moveParams1Attr{};
        moveParams1Attr.binding = 1;
        moveParams1Attr.location = 8;
        moveParams1Attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        moveParams1Attr.offset = 96;  // offsetof(GPUEntity, movementParams1)
        state.vertexAttributes.push_back(moveParams1Attr);
        
        // Default color blend for opaque rendering
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        state.colorBlendAttachments.push_back(colorBlendAttachment);
        
        return state;
    }
}

void GraphicsPipelineManager::optimizeCache(uint64_t currentFrame) {
    // Simple LRU eviction for graphics pipeline cache
    for (auto it = pipelineCache_.begin(); it != pipelineCache_.end();) {
        if (currentFrame - it->second->lastUsedFrame > 1000) { // Evict after 1000 frames
            if (it->second->pipeline != VK_NULL_HANDLE) {
                destroyPipeline(it->second->pipeline);
            }
            if (it->second->layout != VK_NULL_HANDLE) {
                destroyPipelineLayout(it->second->layout);
            }
            it = pipelineCache_.erase(it);
            stats.totalPipelines--;
        } else {
            ++it;
        }
    }
}

void GraphicsPipelineManager::destroyRenderPass(VkRenderPass renderPass) {
    if (renderPass != VK_NULL_HANDLE && context) {
        destroyRenderPass(renderPass);
    }
}

GraphicsPipelineState GraphicsPipelineManager::createMSAAState() {
    // Initialize default MSAA state - this is used by pipeline system manager
    GraphicsPipelineState state{};
    state.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;
    state.sampleShadingEnable = VK_FALSE;
    state.minSampleShading = 1.0f;
    
    // Default color blend for MSAA
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    state.colorBlendAttachments.push_back(colorBlendAttachment);
    
    return state;
}

void GraphicsPipelineManager::warmupCache(const std::vector<GraphicsPipelineState>& commonStates) {
    // Pre-create commonly used pipeline states
    for (const auto& state : commonStates) {
        getPipeline(state); // This will create and cache the pipeline
    }
}