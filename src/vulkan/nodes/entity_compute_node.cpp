#include "entity_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../../ecs/gpu/gpu_entity_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../core/vulkan_constants.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../monitoring/gpu_timeout_detector.h"
#include <iostream>
#include <array>
#include <glm/glm.hpp>
#include <stdexcept>
#include <memory>

namespace {
    // Removed DebugLogger class - using thread-safe atomic counters instead
    
    struct DispatchParams {
        uint32_t totalWorkgroups;
        uint32_t maxWorkgroupsPerChunk;
        bool useChunking;
    };
    
    DispatchParams calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) {
        const uint32_t totalWorkgroups = (entityCount + THREADS_PER_WORKGROUP - 1) / THREADS_PER_WORKGROUP;
        return {
            totalWorkgroups,
            maxWorkgroups,
            totalWorkgroups > maxWorkgroups || forceChunking
        };
    }
}

EntityComputeNode::EntityComputeNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId currentPositionBuffer,
    FrameGraphTypes::ResourceId targetPositionBuffer,
    ComputePipelineManager* computeManager,
    GPUEntityManager* gpuEntityManager,
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , currentPositionBufferId(currentPositionBuffer)
  , targetPositionBufferId(targetPositionBuffer)
  , computeManager(computeManager)
  , gpuEntityManager(gpuEntityManager)
  , timeoutDetector(timeoutDetector) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!computeManager) {
        throw std::invalid_argument("EntityComputeNode: computeManager cannot be null");
    }
    if (!gpuEntityManager) {
        throw std::invalid_argument("EntityComputeNode: gpuEntityManager cannot be null");
    }
}

std::vector<ResourceDependency> EntityComputeNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},
    };
}

std::vector<ResourceDependency> EntityComputeNode::getOutputs() const {
    return {
        {entityBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
    };
}

void EntityComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    // Validate dependencies are still valid
    if (!computeManager || !gpuEntityManager) {
        std::cerr << "EntityComputeNode: Critical error - dependencies became null during execution" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    if (entityCount == 0) {
        FRAME_GRAPH_DEBUG_LOG_THROTTLED(debugCounter, 1800, "EntityComputeNode: No entities to process");
        return;
    }
    
    // Create compute pipeline state for entity movement
    auto layoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
    ComputePipelineState pipelineState = ComputePipelinePresets::createEntityMovementState(descriptorLayout);
    
    // Set frame counter from FrameGraph for compute shader consistency
    pushConstants.frame = frameGraph.getGlobalFrameCounter();
    
    // Create compute dispatch
    ComputeDispatch dispatch{};
    dispatch.pipeline = computeManager->getPipeline(pipelineState);
    
    dispatch.layout = computeManager->getPipelineLayout(pipelineState);
    
    if (dispatch.pipeline == VK_NULL_HANDLE || dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << "EntityComputeNode: Failed to get compute pipeline or layout" << std::endl;
        return;
    }
    
    // Set up descriptor sets
    VkDescriptorSet computeDescriptorSet = gpuEntityManager->getDescriptorManager().getComputeDescriptorSet();
    
    if (computeDescriptorSet != VK_NULL_HANDLE) {
        dispatch.descriptorSets.push_back(computeDescriptorSet);
    } else {
        std::cerr << "EntityComputeNode: ERROR - Missing compute descriptor set!" << std::endl;
        return;
    }
    
    // Configure push constants and dispatch
    pushConstants.entityCount = entityCount;
    dispatch.pushConstantData = &pushConstants;
    dispatch.pushConstantSize = sizeof(ComputePushConstants);
    dispatch.pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
    dispatch.calculateOptimalDispatch(entityCount, glm::uvec3(THREADS_PER_WORKGROUP, 1, 1));
    
    // Apply adaptive workload management
    uint32_t maxWorkgroupsPerDispatch = adaptiveMaxWorkgroups;
    bool shouldForceChunking = forceChunkedDispatch;
    
    if (timeoutDetector) {
        auto recommendation = timeoutDetector->getRecoveryRecommendation();
        if (recommendation.shouldReduceWorkload) {
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, recommendation.recommendedMaxWorkgroups);
        }
        if (recommendation.shouldSplitDispatches) {
            shouldForceChunking = true;
        }
        if (!timeoutDetector->isGPUHealthy()) {
            std::cerr << "EntityComputeNode: GPU not healthy, reducing workload" << std::endl;
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, 512u);
        }
    }
    
    // Calculate dispatch parameters
    auto dispatchParams = calculateDispatchParams(entityCount, maxWorkgroupsPerDispatch, shouldForceChunking);
    
    // Validate dispatch limits
    if (dispatchParams.totalWorkgroups > 65535) {
        std::cerr << "ERROR: Workgroup count " << dispatchParams.totalWorkgroups << " exceeds Vulkan limit!" << std::endl;
        return;
    }
    
    // Debug logging (thread-safe) - once every 30 seconds
    FRAME_GRAPH_DEBUG_LOG_THROTTLED(debugCounter, 1800, "EntityComputeNode (Movement): " << entityCount << " entities → " << dispatchParams.totalWorkgroups << " workgroups");
    
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << "EntityComputeNode: Cannot get Vulkan context" << std::endl;
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
        // Single dispatch execution
        std::cout << "EntityComputeNode: Starting single dispatch execution..." << std::endl;
        if (timeoutDetector) {
            timeoutDetector->beginComputeDispatch("EntityMovement", dispatchParams.totalWorkgroups);
        }
        
        vk.vkCmdPushConstants(
            commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ComputePushConstants), &pushConstants);
        
        vk.vkCmdDispatch(commandBuffer, dispatchParams.totalWorkgroups, 1, 1);
        
        if (timeoutDetector) {
            timeoutDetector->endComputeDispatch();
        }
        
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
    } else {
        executeChunkedDispatch(commandBuffer, context, dispatch, 
                              dispatchParams.totalWorkgroups, dispatchParams.maxWorkgroupsPerChunk, entityCount);
    }
    
}

void EntityComputeNode::executeChunkedDispatch(
    VkCommandBuffer commandBuffer, 
    const VulkanContext* context, 
    const ComputeDispatch& dispatch,
    uint32_t totalWorkgroups,
    uint32_t maxWorkgroupsPerChunk,
    uint32_t entityCount) {
    
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
            std::string chunkName = "EntityMovement_Chunk" + std::to_string(chunkCount);
            timeoutDetector->beginComputeDispatch(chunkName.c_str(), currentChunkSize);
        }
        
        // Update push constants for this chunk
        ComputePushConstants chunkPushConstants = pushConstants;
        chunkPushConstants.entityOffset = baseEntityOffset;
        
        vk.vkCmdPushConstants(
            commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ComputePushConstants), &chunkPushConstants);
        
        vk.vkCmdDispatch(commandBuffer, currentChunkSize, 1, 1);
        
        if (timeoutDetector) {
            timeoutDetector->endComputeDispatch();
        }
        
        // Inter-chunk memory barrier using Synchronization2
        if (processedWorkgroups + currentChunkSize < totalWorkgroups) {
            VkMemoryBarrier2 memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
            memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
            
            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers = &memoryBarrier;
            
            vk.vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        }
        
        processedWorkgroups += currentChunkSize;
        chunkCount++;
    }
    
    // Final memory barrier for compute→graphics synchronization using Synchronization2
    VkMemoryBarrier2 finalMemoryBarrier{};
    finalMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    finalMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    finalMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    finalMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    finalMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;  // SoA reads from storage buffers
    
    VkDependencyInfo finalDependencyInfo{};
    finalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    finalDependencyInfo.memoryBarrierCount = 1;
    finalDependencyInfo.pMemoryBarriers = &finalMemoryBarrier;
    
    vk.vkCmdPipelineBarrier2(commandBuffer, &finalDependencyInfo);
    
    // Debug statistics logging (thread-safe)
    if constexpr (FRAME_GRAPH_DEBUG_ENABLED) {
        uint32_t chunkLogCounter = FrameGraphDebug::incrementCounter(debugCounter);
        if (chunkLogCounter % 300 == 0) {
            std::cout << "[FrameGraph Debug] EntityComputeNode: Split dispatch into " << chunkCount 
                      << " chunks (" << maxWorkgroupsPerChunk << " max) for " << entityCount << " entities (occurrence #" << chunkLogCounter << ")" << std::endl;
            
            if (timeoutDetector) {
                auto stats = timeoutDetector->getStats();
                std::cout << "  GPU Stats: avg=" << stats.averageDispatchTimeMs 
                          << "ms, peak=" << stats.peakDispatchTimeMs << "ms"
                          << ", warnings=" << stats.warningCount 
                          << ", critical=" << stats.criticalCount << std::endl;
            }
        }
    }
}

// Node lifecycle implementation
bool EntityComputeNode::initializeNode(const FrameGraph& frameGraph) {
    // One-time initialization - validate dependencies
    if (!computeManager) {
        std::cerr << "EntityComputeNode: ComputePipelineManager is null" << std::endl;
        return false;
    }
    if (!gpuEntityManager) {
        std::cerr << "EntityComputeNode: GPUEntityManager is null" << std::endl;
        return false;
    }
    return true;
}

void EntityComputeNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    // Store timing data for execution
    currentTime = time;
    currentDeltaTime = deltaTime;
    
    // Update push constants with timing data - frame counter will be set in execute()
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
}

void EntityComputeNode::releaseFrame(uint32_t frameIndex) {
    // Per-frame cleanup - nothing to clean up for compute node
}


