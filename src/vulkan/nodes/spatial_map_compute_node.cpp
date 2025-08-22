#include "spatial_map_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../../ecs/gpu/gpu_entity_manager.h"
#include "../../ecs/gpu/spatial_map_buffers.h"
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

SpatialMapComputeNode::SpatialMapComputeNode(
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId spatialMapBuffer,
    FrameGraphTypes::ResourceId entityCellBuffer,
    ComputePipelineManager* computeManager,
    GPUEntityManager* gpuEntityManager,
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector
) : positionBufferId(positionBuffer)
  , spatialMapBufferId(spatialMapBuffer)
  , entityCellBufferId(entityCellBuffer)
  , computeManager(computeManager)
  , gpuEntityManager(gpuEntityManager)
  , timeoutDetector(timeoutDetector) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!computeManager) {
        throw std::invalid_argument("SpatialMapComputeNode: computeManager cannot be null");
    }
    if (!gpuEntityManager) {
        throw std::invalid_argument("SpatialMapComputeNode: gpuEntityManager cannot be null");
    }
    
    // Initialize push constants with spatial map configuration
    pushConstants.gridResolution = SpatialMapConfig::GRID_RESOLUTION;
    pushConstants.cellSize = SpatialMapConfig::CELL_SIZE;
    pushConstants.worldSize = SpatialMapConfig::WORLD_SIZE;
    pushConstants.maxEntitiesPerCell = SpatialMapConfig::MAX_ENTITIES_PER_CELL;
    pushConstants.clearMapFirst = clearMapEachFrame ? 1 : 0;
}

std::vector<ResourceDependency> SpatialMapComputeNode::getInputs() const {
    return {
        {positionBufferId, ResourceAccess::Read, PipelineStage::ComputeShader},
    };
}

std::vector<ResourceDependency> SpatialMapComputeNode::getOutputs() const {
    return {
        {spatialMapBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
        {entityCellBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
    };
}

bool SpatialMapComputeNode::initializeNode(const FrameGraph& frameGraph) {
    std::cout << "SpatialMapComputeNode: Initialized with " 
              << SpatialMapConfig::GRID_RESOLUTION << "x" << SpatialMapConfig::GRID_RESOLUTION 
              << " grid (" << SpatialMapConfig::TOTAL_CELLS << " cells)" << std::endl;
    return true;
}

void SpatialMapComputeNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    // Update push constants for current frame
    pushConstants.clearMapFirst = clearMapEachFrame ? 1 : 0;
}

void SpatialMapComputeNode::releaseFrame(uint32_t frameIndex) {
    // No per-frame cleanup needed
}

void SpatialMapComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    // Validate dependencies are still valid
    if (!computeManager || !gpuEntityManager) {
        std::cerr << "SpatialMapComputeNode: Critical error - dependencies became null during execution" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    if (entityCount == 0) {
        uint32_t counter = debugCounter.fetch_add(1, std::memory_order_relaxed);
        if (counter % 60 == 0) {
            std::cout << "SpatialMapComputeNode: No entities to process" << std::endl;
        }
        return;
    }
    
    // Create compute pipeline state for spatial map updates
    auto layoutSpec = DescriptorLayoutPresets::createSpatialMapComputeLayout();
    VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
    ComputePipelineState pipelineState = ComputePipelinePresets::createSpatialMapUpdateState(descriptorLayout);
    
    // Create compute dispatch
    ComputeDispatch dispatch{};
    dispatch.pipeline = computeManager->getPipeline(pipelineState);
    dispatch.layout = computeManager->getPipelineLayout(pipelineState);
    
    if (dispatch.pipeline == VK_NULL_HANDLE || dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << "SpatialMapComputeNode: Failed to get compute pipeline or layout" << std::endl;
        return;
    }
    
    // Set up descriptor sets for spatial map compute
    VkDescriptorSet spatialMapDescriptorSet = gpuEntityManager->getDescriptorManager().getSpatialMapDescriptorSet();
    
    if (spatialMapDescriptorSet != VK_NULL_HANDLE) {
        dispatch.descriptorSets.push_back(spatialMapDescriptorSet);
    } else {
        std::cerr << "SpatialMapComputeNode: ERROR - Missing spatial map descriptor set!" << std::endl;
        return;
    }
    
    // Configure push constants and dispatch
    pushConstants.entityCount = entityCount;
    dispatch.pushConstantData = &pushConstants;
    dispatch.pushConstantSize = sizeof(SpatialPushConstants);
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
            std::cerr << "SpatialMapComputeNode: GPU not healthy, reducing workload" << std::endl;
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, 512u);
        }
    }
    
    // Calculate dispatch parameters
    auto dispatchParams = calculateDispatchParams(entityCount, maxWorkgroupsPerDispatch, shouldForceChunking);
    
    // Validate dispatch limits
    if (dispatchParams.totalWorkgroups > 65535) {
        std::cerr << "ERROR: Spatial map workgroup count " << dispatchParams.totalWorkgroups << " exceeds Vulkan limit!" << std::endl;
        return;
    }
    
    // Execute dispatch
    if (dispatchParams.useChunking) {
        executeChunkedDispatch(commandBuffer, frameGraph.getContext(), dispatch, 
                              dispatchParams.totalWorkgroups, dispatchParams.maxWorkgroupsPerChunk, entityCount);
    } else {
        // Single dispatch
        const auto& vk = frameGraph.getContext()->getLoader();
        
        // Bind pipeline and descriptor sets
        vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
        
        if (!dispatch.descriptorSets.empty()) {
            vk.vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
                                      0, static_cast<uint32_t>(dispatch.descriptorSets.size()),
                                      dispatch.descriptorSets.data(), 0, nullptr);
        }
        
        // Push constants
        if (dispatch.pushConstantData && dispatch.pushConstantSize > 0) {
            vk.vkCmdPushConstants(commandBuffer, dispatch.layout, dispatch.pushConstantStages,
                                 0, dispatch.pushConstantSize, dispatch.pushConstantData);
        }
        
        // Dispatch
        vk.vkCmdDispatch(commandBuffer, dispatch.groupCountX, dispatch.groupCountY, dispatch.groupCountZ);
    }
    
    // Debug logging (periodic)
    uint32_t counter = debugCounter.fetch_add(1, std::memory_order_relaxed);
    if (counter % 120 == 0) { // Every 2 seconds at 60fps
        std::cout << "SpatialMapComputeNode: Updated spatial map for " << entityCount 
                  << " entities (" << dispatchParams.totalWorkgroups << " workgroups)" << std::endl;
    }
}

void SpatialMapComputeNode::executeChunkedDispatch(
    VkCommandBuffer commandBuffer, 
    const VulkanContext* context, 
    const ComputeDispatch& dispatch,
    uint32_t totalWorkgroups,
    uint32_t maxWorkgroupsPerChunk,
    uint32_t entityCount) {
    
    const auto& vk = context->getLoader();
    
    // Bind pipeline and descriptor sets once
    vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
    
    if (!dispatch.descriptorSets.empty()) {
        vk.vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
                                  0, static_cast<uint32_t>(dispatch.descriptorSets.size()),
                                  dispatch.descriptorSets.data(), 0, nullptr);
    }
    
    // Execute in chunks
    uint32_t processedWorkgroups = 0;
    uint32_t chunkIndex = 0;
    
    while (processedWorkgroups < totalWorkgroups) {
        uint32_t workgroupsThisChunk = std::min(maxWorkgroupsPerChunk, totalWorkgroups - processedWorkgroups);
        uint32_t entitiesThisChunk = std::min(workgroupsThisChunk * THREADS_PER_WORKGROUP, 
                                             entityCount - (processedWorkgroups * THREADS_PER_WORKGROUP));
        
        // Update push constants for this chunk (entity offset for chunked processing)
        SpatialPushConstants chunkPushConstants = pushConstants;
        chunkPushConstants.entityCount = entitiesThisChunk;
        // Note: Spatial map clearing should only happen in first chunk
        if (chunkIndex > 0) {
            chunkPushConstants.clearMapFirst = 0;
        }
        
        // Push constants for this chunk
        vk.vkCmdPushConstants(commandBuffer, dispatch.layout, dispatch.pushConstantStages,
                             0, sizeof(SpatialPushConstants), &chunkPushConstants);
        
        // Dispatch this chunk
        vk.vkCmdDispatch(commandBuffer, workgroupsThisChunk, 1, 1);
        
        // Memory barrier between chunks to ensure proper synchronization
        if (processedWorkgroups + workgroupsThisChunk < totalWorkgroups) {
            VkMemoryBarrier memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            
            vk.vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }
        
        processedWorkgroups += workgroupsThisChunk;
        chunkIndex++;
    }
}