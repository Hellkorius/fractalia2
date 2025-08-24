#include "base_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../../ecs/gpu/gpu_entity_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_constants.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../pipelines/compute_dispatcher.h"
#include "../monitoring/gpu_timeout_detector.h"
#include <iostream>
#include <stdexcept>
#include <glm/glm.hpp>

BaseComputeNode::BaseComputeNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId currentPositionBuffer,
    FrameGraphTypes::ResourceId targetPositionBuffer,
    ComputePipelineManager* computeManager,
    GPUEntityManager* gpuEntityManager,
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector,
    const char* nodeTypeName
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , currentPositionBufferId(currentPositionBuffer)
  , targetPositionBufferId(targetPositionBuffer)
  , computeManager(computeManager)
  , gpuEntityManager(gpuEntityManager)
  , timeoutDetector(timeoutDetector)
  , nodeTypeName(nodeTypeName) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!computeManager) {
        throw std::invalid_argument(std::string(nodeTypeName) + ": computeManager cannot be null");
    }
    if (!gpuEntityManager) {
        throw std::invalid_argument(std::string(nodeTypeName) + ": gpuEntityManager cannot be null");
    }
}

void BaseComputeNode::onFirstUse(const FrameGraph& frameGraph) {
    // Additional dependency validation during first use if needed
    if (!validateDependencies()) {
        std::cerr << nodeTypeName << ": Dependency validation failed during first use" << std::endl;
    }
}

void BaseComputeNode::executeComputeNode(
    VkCommandBuffer commandBuffer, 
    const FrameGraph& frameGraph, 
    float time, 
    float deltaTime,
    const char* pipelinePresetName
) {
    // Validate dependencies are still valid
    if (!validateDependencies()) {
        std::cerr << nodeTypeName << ": Critical error - dependencies became null during execution" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    if (entityCount == 0) {
        FRAME_GRAPH_DEBUG_LOG_THROTTLED(debugCounter, 1800, nodeTypeName << ": No entities to process");
        return;
    }
    
    // Create compute pipeline state using Vulkan 1.3 descriptor indexing
    auto layoutSpec = DescriptorLayoutPresets::createEntityIndexedLayout();
    VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
    ComputePipelineState pipelineState = this->createPipelineState(descriptorLayout);
    
    // Setup push constants using derived class implementation
    setupPushConstants(time, deltaTime, entityCount, frameGraph.getGlobalFrameCounter());
    
    // Create compute dispatch
    ComputeDispatch dispatch{};
    dispatch.pipeline = computeManager->getPipeline(pipelineState);
    dispatch.layout = computeManager->getPipelineLayout(pipelineState);
    
    if (dispatch.pipeline == VK_NULL_HANDLE || dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << nodeTypeName << ": Failed to get compute pipeline or layout" << std::endl;
        return;
    }
    
    // Set up descriptor sets (using Vulkan 1.3 indexing)
    VkDescriptorSet computeDescriptorSet = gpuEntityManager->getDescriptorManager().getIndexedDescriptorSet();
    
    if (computeDescriptorSet != VK_NULL_HANDLE) {
        dispatch.descriptorSets.push_back(computeDescriptorSet);
    } else {
        std::cerr << nodeTypeName << ": ERROR - Missing compute descriptor set!" << std::endl;
        return;
    }
    
    // Configure push constants and dispatch
    pushConstants.entityCount = entityCount;
    dispatch.pushConstantData = &pushConstants;
    dispatch.pushConstantSize = sizeof(NodePushConstants);
    dispatch.pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
    dispatch.calculateOptimalDispatch(entityCount, glm::uvec3(THREADS_PER_WORKGROUP, 1, 1));
    
    // Apply adaptive workload management
    uint32_t maxWorkgroupsPerDispatch = adaptiveMaxWorkgroups;
    bool shouldForceChunking = forceChunkedDispatch;
    applyAdaptiveWorkloadManagement(maxWorkgroupsPerDispatch, shouldForceChunking);
    
    // Calculate dispatch parameters using derived class implementation
    auto dispatchParams = calculateDispatchParams(entityCount, maxWorkgroupsPerDispatch, shouldForceChunking);
    
    // Validate dispatch limits
    if (!validateDispatchLimits(dispatchParams.totalWorkgroups)) {
        return;
    }
    
    // Debug logging (thread-safe) - once every 30 seconds
    FRAME_GRAPH_DEBUG_LOG_THROTTLED(debugCounter, 1800, 
        nodeTypeName << " (" << pipelinePresetName << "): " << entityCount << " entities → " << dispatchParams.totalWorkgroups << " workgroups");
    
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << nodeTypeName << ": Cannot get Vulkan context" << std::endl;
        return;
    }
    
    // Cache loader reference for performance
    const auto& vk = context->getLoader();
    
    // Bind pipeline and descriptor sets once
    vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
    
    vk.vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
        0, 1, &dispatch.descriptorSets[0], 0, nullptr);
    
    if (!dispatchParams.useChunking) {
        executeSingleDispatch(commandBuffer, context, dispatch, dispatchParams.totalWorkgroups);
    } else {
        executeChunkedDispatch(commandBuffer, context, dispatch, 
                              dispatchParams.totalWorkgroups, dispatchParams.maxWorkgroupsPerChunk, entityCount);
    }
}

void BaseComputeNode::executeSingleDispatch(
    VkCommandBuffer commandBuffer,
    const VulkanContext* context,
    const ComputeDispatch& dispatch,
    uint32_t totalWorkgroups
) {
    const auto& vk = context->getLoader();
    
    // Single dispatch execution
    std::cout << nodeTypeName << ": Starting single dispatch execution..." << std::endl;
    if (timeoutDetector) {
        timeoutDetector->beginComputeDispatch(getDispatchBaseName(), totalWorkgroups);
    }
    
    vk.vkCmdPushConstants(
        commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(NodePushConstants), &pushConstants);
    
    vk.vkCmdDispatch(commandBuffer, totalWorkgroups, 1, 1);
    
    if (timeoutDetector) {
        timeoutDetector->endComputeDispatch();
    }
    
    // Memory barrier for compute→graphics synchronization
    createMemoryBarrier(commandBuffer, context);
}

void BaseComputeNode::executeChunkedDispatch(
    VkCommandBuffer commandBuffer, 
    const VulkanContext* context, 
    const ComputeDispatch& dispatch,
    uint32_t totalWorkgroups,
    uint32_t maxWorkgroupsPerChunk,
    uint32_t entityCount
) {
    // Cache loader reference for performance
    const auto& vk = context->getLoader();
    
    uint32_t processedWorkgroups = 0;
    uint32_t chunkCount = 0;
    
    while (processedWorkgroups < totalWorkgroups) {
        uint32_t currentChunkSize = std::min(maxWorkgroupsPerChunk, totalWorkgroups - processedWorkgroups);
        uint32_t baseEntityOffset = processedWorkgroups * THREADS_PER_WORKGROUP;
        
        if (entityCount <= baseEntityOffset) break; // No more entities to process
        
        // Monitor chunk execution
        if (timeoutDetector) {
            std::string chunkName = std::string(getDispatchBaseName()) + "_Chunk" + std::to_string(chunkCount);
            timeoutDetector->beginComputeDispatch(chunkName.c_str(), currentChunkSize);
        }
        
        // Update push constants for this chunk
        NodePushConstants chunkPushConstants = pushConstants;
        chunkPushConstants.param1 = baseEntityOffset;  // entityOffset for chunked dispatches
        
        vk.vkCmdPushConstants(
            commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(NodePushConstants), &chunkPushConstants);
        
        vk.vkCmdDispatch(commandBuffer, currentChunkSize, 1, 1);
        
        if (timeoutDetector) {
            timeoutDetector->endComputeDispatch();
        }
        
        // Eliminate inter-chunk barriers for better performance
        // Modern GPUs can handle concurrent workgroup execution without explicit synchronization
        // Only insert barriers when truly necessary for data dependency hazards
        
        processedWorkgroups += currentChunkSize;
        chunkCount++;
    }
    
    // Final memory barrier for compute→graphics synchronization
    createMemoryBarrier(commandBuffer, context);
}

void BaseComputeNode::createMemoryBarrier(VkCommandBuffer commandBuffer, const VulkanContext* context) {
    const auto& vk = context->getLoader();
    
    // Memory barrier for compute→graphics synchronization using Synchronization2
    VkMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;  // SoA reads from storage buffers
    
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    
    vk.vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

bool BaseComputeNode::validateDependencies() const {
    return computeManager != nullptr && gpuEntityManager != nullptr;
}

bool BaseComputeNode::validateDispatchLimits(uint32_t totalWorkgroups) const {
    if (totalWorkgroups > 65535) {
        std::cerr << "ERROR: Workgroup count " << totalWorkgroups << " exceeds Vulkan limit!" << std::endl;
        return false;
    }
    return true;
}

void BaseComputeNode::applyAdaptiveWorkloadManagement(uint32_t& maxWorkgroupsPerDispatch, bool& shouldForceChunking) const {
    if (timeoutDetector) {
        auto recommendation = timeoutDetector->getRecoveryRecommendation();
        if (recommendation.shouldReduceWorkload) {
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, recommendation.recommendedMaxWorkgroups);
        }
        if (recommendation.shouldSplitDispatches) {
            shouldForceChunking = true;
        }
        if (!timeoutDetector->isGPUHealthy()) {
            std::cerr << nodeTypeName << ": GPU not healthy, reducing workload" << std::endl;
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, 512u);
        }
    }
}