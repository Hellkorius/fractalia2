#include "compute_pipeline_manager.h"
#include "shader_manager.h"
#include "vulkan_function_loader.h"
#include "vulkan_utils.h"
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
    size_t hash = 0;
    
    // Hash shader path
    hash ^= std::hash<std::string>{}(shaderPath) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // Hash specialization constants
    for (const auto& constant : specializationConstants) {
        hash ^= std::hash<uint32_t>{}(constant) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash descriptor set layouts
    for (const auto& layout : descriptorSetLayouts) {
        hash ^= std::hash<VkDescriptorSetLayout>{}(layout) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash push constant ranges
    for (const auto& range : pushConstantRanges) {
        hash ^= std::hash<uint32_t>{}(range.stageFlags) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(range.offset) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(range.size) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash workgroup size
    hash ^= std::hash<uint32_t>{}(workgroupSizeX) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(workgroupSizeY) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(workgroupSizeZ) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    return hash;
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
ComputePipelineManager::ComputePipelineManager() {
}

ComputePipelineManager::~ComputePipelineManager() {
    cleanup();
}

bool ComputePipelineManager::initialize(const VulkanContext& context,
                                      ShaderManager* shaderManager,
                                      DescriptorLayoutManager* layoutManager) {
    this->context = &context;
    this->shaderManager = shaderManager;
    this->layoutManager = layoutManager;
    
    // Create pipeline cache for optimal performance
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = 0;
    cacheInfo.pInitialData = nullptr;
    
    VkResult result = context.getLoader().vkCreatePipelineCache(
        context.getDevice(), &cacheInfo, nullptr, &pipelineCache);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline cache: " << result << std::endl;
        return false;
    }
    
    std::cout << "ComputePipelineManager initialized successfully" << std::endl;
    return true;
}

void ComputePipelineManager::cleanup() {
    if (!context) return;
    
    // Wait for any async compilations to complete
    for (auto& [state, future] : asyncCompilations) {
        if (future.valid()) {
            future.wait();
        }
    }
    asyncCompilations.clear();
    
    // Clear pipeline cache
    clearCache();
    
    // Destroy pipeline cache
    if (pipelineCache != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyPipelineCache(context->getDevice(), pipelineCache, nullptr);
        pipelineCache = VK_NULL_HANDLE;
    }
    
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
        return it->second->pipeline;
    }
    
    // Check async compilation
    auto asyncIt = asyncCompilations.find(state);
    if (asyncIt != asyncCompilations.end() && asyncIt->second.valid()) {
        // Check if compilation is done
        if (asyncIt->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto cachedPipeline = asyncIt->second.get();
            asyncCompilations.erase(asyncIt);
            
            if (cachedPipeline) {
                VkPipeline pipeline = cachedPipeline->pipeline;
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

VkPipelineLayout ComputePipelineManager::getPipelineLayout(const ComputePipelineState& state) {
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
    context->getLoader().vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
    
    // Bind descriptor sets
    if (!dispatch.descriptorSets.empty()) {
        context->getLoader().vkCmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
            0, static_cast<uint32_t>(dispatch.descriptorSets.size()),
            dispatch.descriptorSets.data(), 0, nullptr);
    }
    
    // Push constants
    if (dispatch.pushConstantData && dispatch.pushConstantSize > 0) {
        context->getLoader().vkCmdPushConstants(
            commandBuffer, dispatch.layout, dispatch.pushConstantStages,
            0, dispatch.pushConstantSize, dispatch.pushConstantData);
    }
    
    // Insert memory barriers if any are provided
    if (!dispatch.memoryBarriers.empty() || !dispatch.bufferBarriers.empty() || !dispatch.imageBarriers.empty()) {
        insertOptimalBarriers(commandBuffer, dispatch.bufferBarriers, dispatch.imageBarriers);
    }
    
    // Dispatch compute work
    context->getLoader().vkCmdDispatch(commandBuffer, 
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
    cachedPipeline->layout = createPipelineLayout(state.descriptorSetLayouts, state.pushConstantRanges);
    if (cachedPipeline->layout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute pipeline layout" << std::endl;
        return nullptr;
    }
    
    // Load shader through ShaderManager
    VkShaderModule shaderModule = shaderManager->loadSPIRVFromFile(state.shaderPath);
    if (shaderModule == VK_NULL_HANDLE) {
        std::cerr << "Failed to load compute shader: " << state.shaderPath << std::endl;
        if (cachedPipeline->layout != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyPipelineLayout(context->getDevice(), cachedPipeline->layout, nullptr);
        }
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
    pipelineInfo.layout = cachedPipeline->layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    // Create compute pipeline
    VkResult result = context->getLoader().vkCreateComputePipelines(
        context->getDevice(), pipelineCache, 1, &pipelineInfo, nullptr, &cachedPipeline->pipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline: " << result << std::endl;
        if (cachedPipeline->layout != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyPipelineLayout(context->getDevice(), cachedPipeline->layout, nullptr);
        }
        return nullptr;
    }
    
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
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
    layoutInfo.pPushConstantRanges = pushConstants.data();
    
    VkPipelineLayout layout;
    VkResult result = context->getLoader().vkCreatePipelineLayout(
        context->getDevice(), &layoutInfo, nullptr, &layout);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline layout: " << result << std::endl;
        return VK_NULL_HANDLE;
    }
    
    return layout;
}

void ComputePipelineManager::clearCache() {
    if (!context) return;
    
    // Destroy all cached pipelines
    for (auto& [state, pipeline] : pipelineCache_) {
        if (pipeline->pipeline != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyPipeline(context->getDevice(), pipeline->pipeline, nullptr);
        }
        if (pipeline->layout != VK_NULL_HANDLE) {
            context->getLoader().vkDestroyPipelineLayout(context->getDevice(), pipeline->layout, nullptr);
        }
    }
    
    pipelineCache_.clear();
    stats.totalPipelines = 0;
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
    
    // Destroy the pipeline
    if (lruIt->second->pipeline != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyPipeline(context->getDevice(), lruIt->second->pipeline, nullptr);
    }
    if (lruIt->second->layout != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyPipelineLayout(context->getDevice(), lruIt->second->layout, nullptr);
    }
    
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
    
    context->getLoader().vkCmdPipelineBarrier(
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
    // Default to common optimal size for most GPUs
    return glm::uvec3(32, 1, 1);
}

uint32_t ComputePipelineManager::getDeviceMaxComputeWorkgroupInvocations() const {
    // Default Vulkan minimum
    return 1024;
}

bool ComputePipelineManager::deviceSupportsSubgroupOperations() const {
    // TODO: Query actual device features
    return false;
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
            if (it->second->pipeline != VK_NULL_HANDLE) {
                context->getLoader().vkDestroyPipeline(context->getDevice(), it->second->pipeline, nullptr);
            }
            if (it->second->layout != VK_NULL_HANDLE) {
                context->getLoader().vkDestroyPipelineLayout(context->getDevice(), it->second->layout, nullptr);
            }
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