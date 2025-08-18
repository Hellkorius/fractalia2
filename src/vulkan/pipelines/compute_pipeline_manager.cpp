#include "compute_pipeline_manager.h"
#include "shader_manager.h"
#include "descriptor_layout_manager.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_utils.h"
#include "hash_utils.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cassert>
#include <future>
#include <cmath>
#include <glm/glm.hpp>

// ComputePipelineState implementation
bool ComputePipelineState::operator==(const ComputePipelineState& other) const {
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
    
    return shaderPath == other.shaderPath &&
           specializationConstants == other.specializationConstants &&
           descriptorSetLayouts == other.descriptorSetLayouts &&
           workgroupSizeX == other.workgroupSizeX &&
           workgroupSizeY == other.workgroupSizeY &&
           workgroupSizeZ == other.workgroupSizeZ;
}

size_t ComputePipelineState::getHash() const {
    VulkanHash::HashCombiner hasher;
    
    hasher.combine(shaderPath)
          .combineContainer(specializationConstants)
          .combineContainer(descriptorSetLayouts)
          .combine(workgroupSizeX)
          .combine(workgroupSizeY)
          .combine(workgroupSizeZ);
    
    for (const auto& range : pushConstantRanges) {
        hasher.combine(range.stageFlags)
              .combine(range.offset)
              .combine(range.size);
    }
    
    return hasher.get();
}

// ComputeDispatch implementation
void ComputeDispatch::calculateOptimalDispatch(uint32_t dataSize, const glm::uvec3& workgroupSize) {
    // Calculate number of workgroups needed
    uint32_t elementsPerWorkgroup = workgroupSize.x * workgroupSize.y * workgroupSize.z;
    groupCountX = (dataSize + elementsPerWorkgroup - 1) / elementsPerWorkgroup;
    groupCountY = 1;
    groupCountZ = 1;
    
    // For very large dispatches, consider using 2D or 3D layout for better cache performance
    if (groupCountX > 65535) {  // Vulkan limit
        uint32_t sqrtGroups = static_cast<uint32_t>(std::sqrt(groupCountX));
        groupCountX = sqrtGroups;
        groupCountY = (dataSize + (sqrtGroups * elementsPerWorkgroup) - 1) / (sqrtGroups * elementsPerWorkgroup);
        
        if (groupCountY > 65535) {
            uint32_t cbrtGroups = static_cast<uint32_t>(std::cbrt(dataSize / elementsPerWorkgroup));
            groupCountX = cbrtGroups;
            groupCountY = cbrtGroups;
            groupCountZ = (dataSize + (cbrtGroups * cbrtGroups * elementsPerWorkgroup) - 1) / 
                         (cbrtGroups * cbrtGroups * elementsPerWorkgroup);
        }
    }
}

// ComputePipelineManager implementation
ComputePipelineManager::ComputePipelineManager(VulkanContext* ctx) : VulkanManagerBase(ctx) {
}

ComputePipelineManager::~ComputePipelineManager() {
    cleanupBeforeContextDestruction();
}

bool ComputePipelineManager::initialize(ShaderManager* shaderManager,
                                      DescriptorLayoutManager* layoutManager) {
    this->shaderManager = shaderManager;
    this->layoutManager = layoutManager;
    
    // Create pipeline cache for optimal performance
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    pipelineCache = vulkan_raii::create_pipeline_cache(context, &cacheInfo);
    if (!pipelineCache) {
        std::cerr << "Failed to create compute pipeline cache" << std::endl;
        return false;
    }
    
    // Query and cache device properties
    loader->vkGetPhysicalDeviceProperties(context->getPhysicalDevice(), &deviceProperties);
    
    // Note: Device features would require vkGetPhysicalDeviceFeatures to be loaded in VulkanFunctionLoader
    // For now, deviceFeatures remains zero-initialized which is safe
    
    std::cout << "ComputePipelineManager initialized successfully" << std::endl;
    return true;
}

void ComputePipelineManager::cleanup() {
    cleanupBeforeContextDestruction();
}

void ComputePipelineManager::cleanupBeforeContextDestruction() {
    if (!context) return;
    
    // Wait for any async compilations to complete
    for (auto& [state, future] : asyncCompilations) {
        if (future.valid()) {
            future.wait();
        }
    }
    asyncCompilations.clear();
    
    // Clear pipeline cache (RAII handles cleanup automatically)
    clearCache();
    
    // Reset pipeline cache (RAII handles cleanup automatically)
    pipelineCache.reset();
    
    context = nullptr;
}

VkPipeline ComputePipelineManager::getPipeline(const ComputePipelineState& state) {
    // Check cache first
    auto it = pipelineCache_.find(state);
    if (it != pipelineCache_.end()) {
        // Cache hit
        stats.cacheHits++;
        it->second->lastUsedFrame = stats.cacheHits + stats.cacheMisses;  // Rough frame counter
        it->second->useCount++;
        return it->second->pipeline.get();
    }
    
    // Check async compilation
    auto asyncIt = asyncCompilations.find(state);
    if (asyncIt != asyncCompilations.end() && asyncIt->second.valid()) {
        // Check if compilation is done
        if (asyncIt->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto cachedPipeline = asyncIt->second.get();
            asyncCompilations.erase(asyncIt);
            
            if (cachedPipeline) {
                VkPipeline pipeline = cachedPipeline->pipeline.get();
                pipelineCache_[state] = std::move(cachedPipeline);
                stats.totalPipelines++;
                return pipeline;
            }
        }
    }
    
    // Cache miss - create new pipeline synchronously
    stats.cacheMisses++;
    
    auto cachedPipeline = createPipelineInternal(state);
    if (!cachedPipeline) {
        std::cerr << "Failed to create compute pipeline" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    VkPipeline pipeline = cachedPipeline->pipeline.get();
    
    // Store in cache
    pipelineCache_[state] = std::move(cachedPipeline);
    stats.totalPipelines++;
    
    // Check if cache needs cleanup
    if (pipelineCache_.size() > maxCacheSize) {
        evictLeastRecentlyUsed();
    }
    
    return pipeline;
}

VkPipelineLayout ComputePipelineManager::getPipelineLayout(const ComputePipelineState& state) {
    auto it = pipelineCache_.find(state);
    if (it != pipelineCache_.end()) {
        return it->second->layout.get();
    }
    
    // If pipeline doesn't exist, create it
    VkPipeline pipeline = getPipeline(state);
    if (pipeline == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    // Should now be in cache
    it = pipelineCache_.find(state);
    assert(it != pipelineCache_.end());
    return it->second->layout.get();
}

void ComputePipelineManager::dispatch(VkCommandBuffer commandBuffer, const ComputeDispatch& dispatch) {
    stats.dispatchesThisFrame++;
    stats.totalDispatches++;
    
    // Validate dispatch parameters
    if (dispatch.pipeline == VK_NULL_HANDLE) {
        std::cerr << "ComputePipelineManager: Invalid pipeline handle" << std::endl;
        return;
    }
    if (dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << "ComputePipelineManager: Invalid pipeline layout handle" << std::endl;
        return;
    }
    if (dispatch.groupCountX == 0 || dispatch.groupCountY == 0 || dispatch.groupCountZ == 0) {
        std::cerr << "ComputePipelineManager: Invalid dispatch size: " 
                  << dispatch.groupCountX << "x" << dispatch.groupCountY << "x" << dispatch.groupCountZ << std::endl;
        return;
    }
    
    // Bind pipeline
    cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
    
    // Bind descriptor sets
    if (!dispatch.descriptorSets.empty()) {
        cmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
            0, static_cast<uint32_t>(dispatch.descriptorSets.size()),
            dispatch.descriptorSets.data(), 0, nullptr);
    }
    
    // Push constants
    if (dispatch.pushConstantData && dispatch.pushConstantSize > 0) {
        cmdPushConstants(
            commandBuffer, dispatch.layout, dispatch.pushConstantStages,
            0, dispatch.pushConstantSize, dispatch.pushConstantData);
    }
    
    // Insert memory barriers if any are provided
    if (!dispatch.memoryBarriers.empty() || !dispatch.bufferBarriers.empty() || !dispatch.imageBarriers.empty()) {
        insertOptimalBarriers(commandBuffer, dispatch.bufferBarriers, dispatch.imageBarriers);
    }
    
    // Dispatch compute work
    cmdDispatch(commandBuffer, 
               dispatch.groupCountX, 
               dispatch.groupCountY, 
               dispatch.groupCountZ);
}

void ComputePipelineManager::dispatchBuffer(VkCommandBuffer commandBuffer, 
                                          const ComputePipelineState& state,
                                          uint32_t elementCount, 
                                          const std::vector<VkDescriptorSet>& descriptorSets,
                                          const void* pushConstants, 
                                          uint32_t pushConstantSize) {
    VkPipeline pipeline = getPipeline(state);
    VkPipelineLayout layout = getPipelineLayout(state);
    
    if (pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
        std::cerr << "Failed to get compute pipeline for buffer dispatch" << std::endl;
        return;
    }
    
    // Create optimized dispatch
    ComputeDispatch dispatch{};
    dispatch.pipeline = pipeline;
    dispatch.layout = layout;
    dispatch.descriptorSets = descriptorSets;
    dispatch.pushConstantData = pushConstants;
    dispatch.pushConstantSize = pushConstantSize;
    
    // Calculate optimal workgroup configuration
    glm::uvec3 workgroupSize(state.workgroupSizeX, state.workgroupSizeY, state.workgroupSizeZ);
    dispatch.calculateOptimalDispatch(elementCount, workgroupSize);
    
    this->dispatch(commandBuffer, dispatch);
}

void ComputePipelineManager::dispatchImage(VkCommandBuffer commandBuffer,
                                         const ComputePipelineState& state,
                                         uint32_t width, uint32_t height,
                                         const std::vector<VkDescriptorSet>& descriptorSets,
                                         const void* pushConstants,
                                         uint32_t pushConstantSize) {
    VkPipeline pipeline = getPipeline(state);
    VkPipelineLayout layout = getPipelineLayout(state);
    
    if (pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
        std::cerr << "Failed to get compute pipeline for image dispatch" << std::endl;
        return;
    }
    
    // Create optimized dispatch for 2D image processing
    ComputeDispatch dispatch{};
    dispatch.pipeline = pipeline;
    dispatch.layout = layout;
    dispatch.descriptorSets = descriptorSets;
    dispatch.pushConstantData = pushConstants;
    dispatch.pushConstantSize = pushConstantSize;
    
    // Calculate 2D dispatch
    dispatch.groupCountX = (width + state.workgroupSizeX - 1) / state.workgroupSizeX;
    dispatch.groupCountY = (height + state.workgroupSizeY - 1) / state.workgroupSizeY;
    dispatch.groupCountZ = 1;
    
    this->dispatch(commandBuffer, dispatch);
}

std::unique_ptr<CachedComputePipeline> ComputePipelineManager::createPipelineInternal(const ComputePipelineState& state) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!validatePipelineState(state)) {
        std::cerr << "Invalid compute pipeline state" << std::endl;
        return nullptr;
    }
    
    auto cachedPipeline = std::make_unique<CachedComputePipeline>();
    cachedPipeline->state = state;
    
    // Create pipeline layout
    VkPipelineLayout rawLayout = createPipelineLayout(state.descriptorSetLayouts, state.pushConstantRanges);
    if (rawLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute pipeline layout" << std::endl;
        return nullptr;
    }
    cachedPipeline->layout = vulkan_raii::make_pipeline_layout(rawLayout, context);
    
    // Load shader through ShaderManager
    VkShaderModule shaderModule = shaderManager->loadSPIRVFromFile(state.shaderPath);
    if (shaderModule == VK_NULL_HANDLE) {
        std::cerr << "Failed to load compute shader: " << state.shaderPath << std::endl;
        // RAII layout will cleanup automatically
        return nullptr;
    }
    
    // Create shader stage
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";
    
    // Specialization constants (if any)
    VkSpecializationInfo specializationInfo{};
    std::vector<VkSpecializationMapEntry> mapEntries;
    if (!state.specializationConstants.empty()) {
        for (uint32_t i = 0; i < state.specializationConstants.size(); ++i) {
            VkSpecializationMapEntry entry{};
            entry.constantID = i;
            entry.offset = i * sizeof(uint32_t);
            entry.size = sizeof(uint32_t);
            mapEntries.push_back(entry);
        }
        
        specializationInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
        specializationInfo.pMapEntries = mapEntries.data();
        specializationInfo.dataSize = state.specializationConstants.size() * sizeof(uint32_t);
        specializationInfo.pData = state.specializationConstants.data();
        
        shaderStageInfo.pSpecializationInfo = &specializationInfo;
    }
    
    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = cachedPipeline->layout.get();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    // Create compute pipeline with detailed error logging
    std::cout << "ComputePipelineManager: Creating compute pipeline for shader: " << state.shaderPath << std::endl;
    std::cout << "  Pipeline layout: " << (void*)cachedPipeline->layout.get() << std::endl;
    std::cout << "  Pipeline cache: " << (void*)pipelineCache.get() << std::endl;
    std::cout << "  Shader module: " << (void*)shaderModule << std::endl;
    
    VkPipeline rawPipeline;
    VkResult result = createComputePipelines(pipelineCache.get(), 1, &pipelineInfo, &rawPipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "ComputePipelineManager: CRITICAL ERROR - Failed to create compute pipeline!" << std::endl;
        std::cerr << "  Shader path: " << state.shaderPath << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        std::cerr << "  Pipeline layout valid: " << (cachedPipeline->layout ? "YES" : "NO") << std::endl;
        std::cerr << "  Pipeline cache valid: " << (pipelineCache ? "YES" : "NO") << std::endl;
        std::cerr << "  Shader module valid: " << (shaderModule != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        std::cerr << "  Device valid: " << (context->getDevice() != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        
        // RAII layout will cleanup automatically
        return nullptr;
    } else {
        std::cout << "ComputePipelineManager: Compute pipeline created successfully" << std::endl;
    }
    
    cachedPipeline->pipeline = vulkan_raii::make_pipeline(rawPipeline, context);
    
    // Set up dispatch optimization info
    cachedPipeline->dispatchInfo.optimalWorkgroupSize = getDeviceOptimalWorkgroupSize();
    cachedPipeline->dispatchInfo.maxInvocationsPerWorkgroup = getDeviceMaxComputeWorkgroupInvocations();
    cachedPipeline->dispatchInfo.supportsSubgroupOperations = deviceSupportsSubgroupOperations();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    cachedPipeline->compilationTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    stats.totalCompilationTime += cachedPipeline->compilationTime;
    
    logPipelineCreation(state, cachedPipeline->compilationTime);
    
    return cachedPipeline;
}

VkPipelineLayout ComputePipelineManager::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                                                             const std::vector<VkPushConstantRange>& pushConstants) {
    std::cout << "ComputePipelineManager: Creating pipeline layout" << std::endl;
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
        std::cerr << "ComputePipelineManager: CRITICAL ERROR - Failed to create pipeline layout!" << std::endl;
        std::cerr << "  VkResult: " << result << std::endl;
        std::cerr << "  Device valid: " << (context->getDevice() != VK_NULL_HANDLE ? "YES" : "NO") << std::endl;
        return VK_NULL_HANDLE;
    } else {
        std::cout << "ComputePipelineManager: Pipeline layout created successfully: " << (void*)layout << std::endl;
    }
    
    return layout;
}

void ComputePipelineManager::clearCache() {
    if (!context) return;
    
    // Clear all cached pipelines (RAII handles cleanup automatically)
    pipelineCache_.clear();
    stats.totalPipelines = 0;
}

bool ComputePipelineManager::recreatePipelineCache() {
    if (!context) {
        std::cerr << "ComputePipelineManager: Cannot recreate pipeline cache - no context" << std::endl;
        return false;
    }
    
    std::cout << "ComputePipelineManager: CRITICAL FIX - Recreating pipeline cache to prevent second resize corruption" << std::endl;
    
    // Clear existing pipeline objects first
    clearCache();
    
    // CRITICAL FIX: Also clear descriptor layout cache to prevent stale layout handles
    // Descriptor layouts may become invalid after command pool recreation
    if (layoutManager) {
        std::cout << "ComputePipelineManager: Also clearing descriptor layout cache to prevent stale handles" << std::endl;
        layoutManager->clearCache();
    }
    
    // Destroy and recreate the VkPipelineCache object itself
    if (pipelineCache) {
        std::cout << "ComputePipelineManager: Destroying corrupted pipeline cache" << std::endl;
        pipelineCache.reset();
    }
    
    // Create fresh pipeline cache
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    pipelineCache = vulkan_raii::create_pipeline_cache(context, &cacheInfo);
    if (!pipelineCache) {
        std::cerr << "ComputePipelineManager: CRITICAL FAILURE - Failed to recreate pipeline cache" << std::endl;
        return false;
    }
    
    std::cout << "ComputePipelineManager: Pipeline cache successfully recreated" << std::endl;
    return true;
}

void ComputePipelineManager::evictLeastRecentlyUsed() {
    if (pipelineCache_.empty()) return;
    
    // Find least recently used pipeline
    auto lruIt = pipelineCache_.begin();
    for (auto it = pipelineCache_.begin(); it != pipelineCache_.end(); ++it) {
        if (it->second->lastUsedFrame < lruIt->second->lastUsedFrame) {
            lruIt = it;
        }
    }
    
    // Erase the pipeline (RAII handles cleanup automatically)
    pipelineCache_.erase(lruIt);
    stats.totalPipelines--;
}

glm::uvec3 ComputePipelineManager::calculateOptimalWorkgroupSize(uint32_t dataSize,
                                                                const glm::uvec3& maxWorkgroupSize) const {
    // Start with device optimal size
    glm::uvec3 optimal = getDeviceOptimalWorkgroupSize();
    
    // Clamp to device limits
    optimal.x = std::min(optimal.x, maxWorkgroupSize.x);
    optimal.y = std::min(optimal.y, maxWorkgroupSize.y);
    optimal.z = std::min(optimal.z, maxWorkgroupSize.z);
    
    // For 1D data, use optimal X size and 1 for Y, Z
    if (dataSize <= optimal.x * 4) {
        // Small data - use smaller workgroup to avoid underutilization
        optimal.x = std::min(optimal.x, (dataSize + 3) / 4);
        optimal.y = 1;
        optimal.z = 1;
    }
    
    return optimal;
}

void ComputePipelineManager::insertOptimalBarriers(VkCommandBuffer commandBuffer,
                                                   const std::vector<VkBufferMemoryBarrier>& bufferBarriers,
                                                   const std::vector<VkImageMemoryBarrier>& imageBarriers,
                                                   VkPipelineStageFlags srcStage,
                                                   VkPipelineStageFlags dstStage) {
    if (bufferBarriers.empty() && imageBarriers.empty()) {
        return;
    }
    
    // Optimize buffer barriers by merging adjacent ranges
    auto optimizedBufferBarriers = optimizeBufferBarriers(bufferBarriers);
    
    cmdPipelineBarrier(
        commandBuffer,
        srcStage, dstStage,
        0,  // No dependency flags for compute
        0, nullptr,  // No memory barriers
        static_cast<uint32_t>(optimizedBufferBarriers.size()), optimizedBufferBarriers.data(),
        static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data()
    );
}

bool ComputePipelineManager::validatePipelineState(const ComputePipelineState& state) const {
    if (state.shaderPath.empty()) {
        std::cerr << "Compute pipeline state validation failed: empty shader path" << std::endl;
        return false;
    }
    
    if (state.workgroupSizeX == 0 || state.workgroupSizeY == 0 || state.workgroupSizeZ == 0) {
        std::cerr << "Compute pipeline state validation failed: invalid workgroup size" << std::endl;
        return false;
    }
    
    return true;
}

void ComputePipelineManager::logPipelineCreation(const ComputePipelineState& state,
                                                std::chrono::nanoseconds compilationTime) const {
    std::cout << "Created compute pipeline: " << state.shaderPath 
              << " (compilation time: " << compilationTime.count() / 1000000.0f << "ms)" << std::endl;
}

void ComputePipelineManager::resetFrameStats() {
    stats.dispatchesThisFrame = 0;
    stats.hitRatio = static_cast<float>(stats.cacheHits) / static_cast<float>(stats.cacheHits + stats.cacheMisses);
}

// Device capability queries (placeholders - need proper implementation)
glm::uvec3 ComputePipelineManager::getDeviceOptimalWorkgroupSize() const {
    // Use device limits to determine optimal workgroup size
    // Most GPUs perform well with 32 or 64 threads per workgroup
    uint32_t maxWorkgroupSize = std::min(deviceProperties.limits.maxComputeWorkGroupInvocations, 64u);
    return glm::uvec3(maxWorkgroupSize, 1, 1);
}

uint32_t ComputePipelineManager::getDeviceMaxComputeWorkgroupInvocations() const {
    return deviceProperties.limits.maxComputeWorkGroupInvocations;
}

bool ComputePipelineManager::deviceSupportsSubgroupOperations() const {
    // Check for subgroup operations support
    // Note: Basic subgroup support is part of Vulkan 1.1 core
    // For more advanced subgroup features, we'd need to check VkPhysicalDeviceSubgroupProperties
    return true;  // Assume Vulkan 1.1+ support
}

std::vector<VkBufferMemoryBarrier> ComputePipelineManager::optimizeBufferBarriers(
    const std::vector<VkBufferMemoryBarrier>& barriers) const {
    if (barriers.empty()) {
        return {};
    }
    
    if (barriers.size() == 1) {
        return {barriers[0]};
    }
    
    // For now, return as-is. Advanced optimization would merge adjacent ranges
    std::vector<VkBufferMemoryBarrier> result;
    result.reserve(barriers.size());
    for (const auto& barrier : barriers) {
        result.push_back(barrier);
    }
    return result;
}

ComputePipelineState ComputePipelineManager::createBufferProcessingState(const std::string& shaderPath,
                                                                       VkDescriptorSetLayout descriptorLayout) {
    ComputePipelineState state{};
    state.shaderPath = shaderPath;
    state.descriptorSetLayouts.push_back(descriptorLayout);
    state.workgroupSizeX = 64;  // Good for buffer processing
    state.workgroupSizeY = 1;
    state.workgroupSizeZ = 1;
    state.isFrequentlyUsed = true;
    
    return state;
}

// ComputePipelinePresets namespace implementation
namespace ComputePipelinePresets {
    ComputePipelineState createEntityMovementState(VkDescriptorSetLayout descriptorLayout) {
        ComputePipelineState state{};
        state.shaderPath = "shaders/movement_random.comp.spv";
        state.descriptorSetLayouts.push_back(descriptorLayout);
        state.workgroupSizeX = 64;  // MUST match shader local_size_x = 64
        state.workgroupSizeY = 1;
        state.workgroupSizeZ = 1;
        state.isFrequentlyUsed = true;
        
        // Add push constants for time/frame data (must match ComputePushConstants struct)
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(float) * 2 + sizeof(uint32_t) * 6;  // time, deltaTime, entityCount, frame, entityOffset, padding[3]
        state.pushConstantRanges.push_back(pushConstant);
        
        return state;
    }
}

void ComputePipelineManager::optimizeCache(uint64_t currentFrame) {
    // Simple LRU eviction for compute pipeline cache
    for (auto it = pipelineCache_.begin(); it != pipelineCache_.end();) {
        if (currentFrame - it->second->lastUsedFrame > 1000) { // Evict after 1000 frames
            // Erase pipeline (RAII handles cleanup automatically)
            it = pipelineCache_.erase(it);
            stats.totalPipelines--;
        } else {
            ++it;
        }
    }
}

void ComputePipelineManager::warmupCache(const std::vector<ComputePipelineState>& commonStates) {
    // Pre-create commonly used compute pipeline states
    for (const auto& state : commonStates) {
        getPipeline(state); // This will create and cache the pipeline
    }
}