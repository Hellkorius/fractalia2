#include "pipeline_system_manager.h"
#include <iostream>

PipelineSystemManager::PipelineSystemManager() {
}

PipelineSystemManager::~PipelineSystemManager() {
    cleanup();
}

bool PipelineSystemManager::initialize(const VulkanContext& context) {
    this->context = &context;
    
    std::cout << "Initializing AAA Pipeline System Manager..." << std::endl;
    
    if (!initializeManagers()) {
        std::cerr << "Failed to initialize pipeline managers" << std::endl;
        return false;
    }
    
    std::cout << "AAA Pipeline System Manager initialized successfully" << std::endl;
    return true;
}

void PipelineSystemManager::cleanup() {
    if (!context) return;
    
    // Cleanup in reverse order of initialization
    computeManager.reset();
    graphicsManager.reset();
    layoutManager.reset();
    shaderManager.reset();
    
    context = nullptr;
}

bool PipelineSystemManager::initializeManagers() {
    // Initialize shader manager first (required by pipeline managers)
    shaderManager = std::make_unique<ShaderManager>();
    if (!shaderManager->initialize(*context)) {
        std::cerr << "Failed to initialize ShaderManager" << std::endl;
        return false;
    }
    
    // Initialize descriptor layout manager (required by pipeline managers)
    layoutManager = std::make_unique<DescriptorLayoutManager>();
    if (!layoutManager->initialize(*context)) {
        std::cerr << "Failed to initialize DescriptorLayoutManager" << std::endl;
        return false;
    }
    
    // Initialize graphics pipeline manager
    graphicsManager = std::make_unique<GraphicsPipelineManager>();
    if (!graphicsManager->initialize(*context, shaderManager.get(), layoutManager.get())) {
        std::cerr << "Failed to initialize GraphicsPipelineManager" << std::endl;
        return false;
    }
    
    // Initialize compute pipeline manager
    computeManager = std::make_unique<ComputePipelineManager>();
    if (!computeManager->initialize(*context, shaderManager.get(), layoutManager.get())) {
        std::cerr << "Failed to initialize ComputePipelineManager" << std::endl;
        return false;
    }
    
    return true;
}

VkPipeline PipelineSystemManager::createGraphicsPipeline(const PipelineCreationInfo& info) {
    if (!graphicsManager || !shaderManager || !layoutManager) {
        std::cerr << "Pipeline managers not initialized" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Load shaders
    VkShaderModule vertexShader = VK_NULL_HANDLE;
    VkShaderModule fragmentShader = VK_NULL_HANDLE;
    
    if (!info.vertexShaderPath.empty()) {
        vertexShader = shaderManager->loadShaderFromFile(info.vertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
        if (vertexShader == VK_NULL_HANDLE) {
            std::cerr << "Failed to load vertex shader: " << info.vertexShaderPath << std::endl;
            return VK_NULL_HANDLE;
        }
    }
    
    if (!info.fragmentShaderPath.empty()) {
        fragmentShader = shaderManager->loadShaderFromFile(info.fragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (fragmentShader == VK_NULL_HANDLE) {
            std::cerr << "Failed to load fragment shader: " << info.fragmentShaderPath << std::endl;
            return VK_NULL_HANDLE;
        }
    }
    
    // Create descriptor layout for this pipeline
    auto layoutSpec = DescriptorLayoutPresets::createEntityGraphicsLayout();
    VkDescriptorSetLayout descriptorLayout = layoutManager->getLayout(layoutSpec);
    if (descriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create descriptor layout" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Create graphics pipeline state
    GraphicsPipelineState pipelineState = GraphicsPipelinePresets::createEntityRenderingState(
        info.renderPass, descriptorLayout);
    
    // Configure MSAA if requested
    if (info.enableMSAA) {
        pipelineState.rasterizationSamples = info.samples;
    }
    
    // Get pipeline from graphics manager
    return graphicsManager->getPipeline(pipelineState);
}

VkPipeline PipelineSystemManager::createComputePipeline(const std::string& computeShaderPath) {
    if (!computeManager || !shaderManager || !layoutManager) {
        std::cerr << "Pipeline managers not initialized" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Load compute shader
    VkShaderModule computeShader = shaderManager->loadShaderFromFile(computeShaderPath, VK_SHADER_STAGE_COMPUTE_BIT);
    if (computeShader == VK_NULL_HANDLE) {
        std::cerr << "Failed to load compute shader: " << computeShaderPath << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Create descriptor layout for compute pipeline
    auto layoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout descriptorLayout = layoutManager->getLayout(layoutSpec);
    if (descriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute descriptor layout" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Create compute pipeline state
    ComputePipelineState pipelineState = ComputePipelinePresets::createEntityMovementState(descriptorLayout);
    pipelineState.shaderPath = computeShaderPath;
    
    // Get pipeline from compute manager
    return computeManager->getPipeline(pipelineState);
}

void PipelineSystemManager::warmupCommonPipelines() {
    if (!graphicsManager || !computeManager || !layoutManager) {
        return;
    }
    
    std::cout << "Warming up common pipelines..." << std::endl;
    
    // Warmup common descriptor layouts
    std::vector<DescriptorLayoutSpec> commonLayouts = {
        DescriptorLayoutPresets::createEntityGraphicsLayout(),
        DescriptorLayoutPresets::createEntityComputeLayout()
    };
    layoutManager->warmupCache(commonLayouts);
    
    // Warmup common graphics pipeline states
    std::vector<GraphicsPipelineState> commonGraphicsStates = {
        graphicsManager->createDefaultState(),
        graphicsManager->createMSAAState()
    };
    // Note: Would need render pass to actually warmup - this is a placeholder
    
    // Warmup common compute pipeline states
    auto entityComputeLayout = layoutManager->getLayout(DescriptorLayoutPresets::createEntityComputeLayout());
    std::vector<ComputePipelineState> commonComputeStates = {
        ComputePipelinePresets::createEntityMovementState(entityComputeLayout)
    };
    computeManager->warmupCache(commonComputeStates);
    
    std::cout << "Pipeline warmup complete" << std::endl;
}

void PipelineSystemManager::optimizeCaches(uint64_t currentFrame) {
    if (graphicsManager) {
        graphicsManager->optimizeCache(currentFrame);
    }
    if (computeManager) {
        computeManager->optimizeCache(currentFrame);
    }
    if (layoutManager) {
        layoutManager->optimizeCache(currentFrame);
    }
    if (shaderManager) {
        shaderManager->optimizeCache(currentFrame);
    }
}

void PipelineSystemManager::resetFrameStats() {
    if (graphicsManager) {
        graphicsManager->resetFrameStats();
    }
    if (computeManager) {
        computeManager->resetFrameStats();
    }
    if (layoutManager) {
        layoutManager->resetFrameStats();
    }
    if (shaderManager) {
        shaderManager->resetFrameStats();
    }
}

PipelineSystemManager::SystemStats PipelineSystemManager::getStats() const {
    SystemStats stats{};
    
    if (graphicsManager) {
        stats.graphics = graphicsManager->getStats();
    }
    if (computeManager) {
        stats.compute = computeManager->getStats();
    }
    if (layoutManager) {
        stats.layouts = layoutManager->getStats();
    }
    if (shaderManager) {
        stats.shaders = shaderManager->getStats();
    }
    
    return stats;
}

// Legacy compatibility methods
VkRenderPass PipelineSystemManager::createRenderPass(VkFormat colorFormat, VkSampleCountFlagBits samples, bool enableMSAA) {
    if (!graphicsManager) {
        std::cerr << "Graphics manager not initialized" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    currentRenderPass = graphicsManager->createRenderPass(colorFormat, VK_FORMAT_UNDEFINED, samples, enableMSAA);
    return currentRenderPass;
}

bool PipelineSystemManager::recreateRenderPass(VkFormat newFormat) {
    if (!graphicsManager) {
        std::cout << "PipelineSystemManager: ERROR - No graphics manager" << std::endl;
        return false;
    }
    
    std::cout << "PipelineSystemManager: Starting render pass recreation..." << std::endl;
    
    // CRITICAL FIX FOR SECOND RESIZE CRASH: Clear cache BEFORE destroying currentRenderPass
    // clearCache() destroys ALL render passes including the one currentRenderPass points to
    // This prevents double-destruction which was causing the second resize crash
    std::cout << "PipelineSystemManager: About to clear graphics pipeline cache..." << std::endl;
    graphicsManager->clearCache();
    std::cout << "PipelineSystemManager: Cleared graphics pipeline cache (including render passes)" << std::endl;
    
    // IMPORTANT: Set currentRenderPass to null since clearCache() already destroyed it
    // This prevents the redundant destroy call that was causing the crash
    if (currentRenderPass != VK_NULL_HANDLE) {
        std::cout << "PipelineSystemManager: Current render pass already destroyed by clearCache(), setting to null" << std::endl;
        currentRenderPass = VK_NULL_HANDLE;
    }
    
    // Create new render pass
    std::cout << "PipelineSystemManager: About to create new render pass with format " << newFormat << std::endl;
    currentRenderPass = graphicsManager->createRenderPass(newFormat, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_2_BIT, true);
    if (currentRenderPass != VK_NULL_HANDLE) {
        std::cout << "PipelineSystemManager: New render pass created successfully: " << (void*)currentRenderPass << std::endl;
    } else {
        std::cout << "PipelineSystemManager: ERROR - Failed to create new render pass" << std::endl;
    }
    return currentRenderPass != VK_NULL_HANDLE;
}

bool PipelineSystemManager::recreateAllPipelineCaches() {
    if (!context) {
        std::cerr << "PipelineSystemManager: Cannot recreate pipeline caches - not initialized" << std::endl;
        return false;
    }
    
    std::cout << "PipelineSystemManager: CRITICAL FIX - Recreating all pipeline caches for second resize crash prevention" << std::endl;
    
    bool success = true;
    
    // Recreate graphics pipeline cache
    if (graphicsManager && !graphicsManager->recreatePipelineCache()) {
        std::cerr << "PipelineSystemManager: Failed to recreate graphics pipeline cache" << std::endl;
        success = false;
    }
    
    // Recreate compute pipeline cache  
    if (computeManager && !computeManager->recreatePipelineCache()) {
        std::cerr << "PipelineSystemManager: Failed to recreate compute pipeline cache" << std::endl;
        success = false;
    }
    
    if (success) {
        std::cout << "PipelineSystemManager: All pipeline caches successfully recreated" << std::endl;
    } else {
        std::cerr << "PipelineSystemManager: CRITICAL ERROR - Failed to recreate some pipeline caches" << std::endl;
    }
    
    return success;
}