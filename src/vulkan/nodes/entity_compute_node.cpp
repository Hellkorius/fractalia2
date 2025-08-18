#include "entity_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../monitoring/gpu_timeout_detector.h"
#include <iostream>
#include <array>
#include <glm/glm.hpp>

namespace {
    // Centralized debug logging with configurable intervals
    class DebugLogger {
    public:
        static void logPeriodic(const std::string& message, int& counter, int interval = 300) {
            if (counter++ % interval == 0) {
                std::cout << message << std::endl;
            }
        }
        
        static void logOnce(const std::string& message, int& counter, int interval = 60) {
            if (counter++ % interval == 0) {
                std::cout << message << std::endl;
            }
        }
    };
    
    // Helper function for dispatch calculation
    struct DispatchParams {
        uint32_t totalWorkgroups;
        uint32_t maxWorkgroupsPerChunk;
        bool useChunking;
    };
    
    DispatchParams calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) {
        const uint32_t totalWorkgroups = (entityCount + 63) / 64; // 64 threads per workgroup
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
}

std::vector<ResourceDependency> EntityComputeNode::getInputs() const {
    return {
        // Entity buffer is read/write - declare as ReadWrite in inputs only
        {entityBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},
        // Current and target position buffers are read/write for interpolation
        {currentPositionBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},
        {targetPositionBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},
    };
}

std::vector<ResourceDependency> EntityComputeNode::getOutputs() const {
    return {
        // Position buffer receives the final computed positions (write-only)
        {positionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
        // Don't list ReadWrite resources here - they're already in inputs
    };
}

void EntityComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    static int frameCount = 0;
    static int noEntitiesCounter = 0;
    frameCount++;
    
    if (!computeManager || !gpuEntityManager) {
        std::cerr << "EntityComputeNode: Missing dependencies" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    if (entityCount == 0) {
        DebugLogger::logOnce("EntityComputeNode: No entities to process", noEntitiesCounter);
        return;
    }
    
    // Create compute pipeline state for entity movement
    auto layoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
    ComputePipelineState pipelineState = ComputePipelinePresets::createEntityMovementState(descriptorLayout);
    
    // Create compute dispatch
    ComputeDispatch dispatch{};
    dispatch.pipeline = computeManager->getPipeline(pipelineState);
    
    dispatch.layout = computeManager->getPipelineLayout(pipelineState);
    
    if (dispatch.pipeline == VK_NULL_HANDLE || dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << "EntityComputeNode: Failed to get compute pipeline or layout" << std::endl;
        return;
    }
    
    // Set up descriptor sets
    VkDescriptorSet computeDescriptorSet = gpuEntityManager->getComputeDescriptorSet();
    
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
    dispatch.calculateOptimalDispatch(entityCount, glm::uvec3(64, 1, 1));
    
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
    
    // Debug logging
    static int debugCounter = 0;
    DebugLogger::logPeriodic(
        "EntityComputeNode: " + std::to_string(entityCount) + " entities → " + 
        std::to_string(dispatchParams.totalWorkgroups) + " workgroups (frame " + std::to_string(frameCount) + ")",
        debugCounter
    );
    
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << "EntityComputeNode: Cannot get Vulkan context" << std::endl;
        return;
    }
    
    // Bind pipeline and descriptor sets once
    context->getLoader().vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
    
    context->getLoader().vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
        0, 1, &dispatch.descriptorSets[0], 0, nullptr);
    
    if (!dispatchParams.useChunking) {
        // Single dispatch execution
        std::cout << "EntityComputeNode: Starting single dispatch execution..." << std::endl;
        if (timeoutDetector) {
            timeoutDetector->beginComputeDispatch("EntityMovement", dispatchParams.totalWorkgroups);
        }
        
        context->getLoader().vkCmdPushConstants(
            commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ComputePushConstants), &pushConstants);
        
        context->getLoader().vkCmdDispatch(commandBuffer, dispatchParams.totalWorkgroups, 1, 1);
        
        if (timeoutDetector) {
            timeoutDetector->endComputeDispatch();
        }
        
        // Memory barrier for compute→graphics synchronization
        VkMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        
        context->getLoader().vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
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
    
    uint32_t processedWorkgroups = 0;
    uint32_t chunkCount = 0;
    
    while (processedWorkgroups < totalWorkgroups) {
        uint32_t currentChunkSize = std::min(maxWorkgroupsPerChunk, totalWorkgroups - processedWorkgroups);
        uint32_t baseEntityOffset = processedWorkgroups * 64;
        
        if (entityCount <= baseEntityOffset) break; // No more entities to process
        
        // Monitor chunk execution
        if (timeoutDetector) {
            std::string chunkName = "EntityMovement_Chunk" + std::to_string(chunkCount);
            timeoutDetector->beginComputeDispatch(chunkName.c_str(), currentChunkSize);
        }
        
        // Update push constants for this chunk
        ComputePushConstants chunkPushConstants = pushConstants;
        chunkPushConstants.entityOffset = baseEntityOffset;
        
        context->getLoader().vkCmdPushConstants(
            commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ComputePushConstants), &chunkPushConstants);
        
        context->getLoader().vkCmdDispatch(commandBuffer, currentChunkSize, 1, 1);
        
        if (timeoutDetector) {
            timeoutDetector->endComputeDispatch();
        }
        
        // Inter-chunk memory barrier
        if (processedWorkgroups + currentChunkSize < totalWorkgroups) {
            VkMemoryBarrier memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            
            context->getLoader().vkCmdPipelineBarrier(
                commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }
        
        processedWorkgroups += currentChunkSize;
        chunkCount++;
    }
    
    // Final memory barrier for compute→graphics synchronization
    VkMemoryBarrier finalMemoryBarrier{};
    finalMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    finalMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    finalMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    context->getLoader().vkCmdPipelineBarrier(
        commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &finalMemoryBarrier, 0, nullptr, 0, nullptr);
    
    // Debug statistics logging
    static int chunkDebugCounter = 0;
    if (chunkDebugCounter++ % 300 == 0) {
        std::string message = "EntityComputeNode: Split dispatch into " + std::to_string(chunkCount) + 
                             " chunks (" + std::to_string(maxWorkgroupsPerChunk) + " max) for " + 
                             std::to_string(entityCount) + " entities";
        std::cout << message << std::endl;
        
        if (timeoutDetector) {
            auto stats = timeoutDetector->getStats();
            std::cout << "  GPU Stats: avg=" << stats.averageDispatchTimeMs 
                      << "ms, peak=" << stats.peakDispatchTimeMs << "ms"
                      << ", warnings=" << stats.warningCount 
                      << ", critical=" << stats.criticalCount << std::endl;
        }
    }
}

void EntityComputeNode::updateFrameData(float time, float deltaTime, uint32_t frameCounter) {
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
    pushConstants.frame = frameCounter;
}

