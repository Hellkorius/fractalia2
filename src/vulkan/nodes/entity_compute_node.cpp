#include "entity_compute_node.h"
#include "../compute_pipeline_manager.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../vulkan_context.h"
#include "../vulkan_function_loader.h"
#include "../descriptor_layout_manager.h"
#include "../gpu_timeout_detector.h"
#include <iostream>
#include <array>
#include <glm/glm.hpp>

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
    // FULL COMPUTE SYSTEM - but with better error handling
    static int frameCount = 0;
    frameCount++;
    
    if (!computeManager || !gpuEntityManager) {
        std::cerr << "EntityComputeNode: Missing dependencies" << std::endl;
        return;
    }
    
    // Only dispatch if we have entities to process
    if (gpuEntityManager->getEntityCount() == 0) {
        static int noEntitiesCounter = 0;
        if (noEntitiesCounter++ % 60 == 0) {
            std::cout << "EntityComputeNode: No entities to process" << std::endl;
        }
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
    
    // Use actual entity count - buffers are sized for MAX_ENTITIES (131k)
    uint32_t actualEntityCount = gpuEntityManager->getEntityCount();
    
    // Use full entity count with proper chunking
    uint32_t entityCount = actualEntityCount;
    
    // Update push constants with current frame data
    pushConstants.entityCount = entityCount;
    dispatch.pushConstantData = &pushConstants;
    dispatch.pushConstantSize = sizeof(ComputePushConstants);
    dispatch.pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Calculate optimal dispatch size with new workgroup size (64 threads)
    dispatch.calculateOptimalDispatch(entityCount, glm::uvec3(64, 1, 1));
    
    // Adaptive workload distribution based on GPU performance monitoring
    uint32_t maxWorkgroupsPerDispatch = adaptiveMaxWorkgroups;
    
    // Apply timeout detector recommendations if available
    if (timeoutDetector) {
        auto recommendation = timeoutDetector->getRecoveryRecommendation();
        if (recommendation.shouldReduceWorkload) {
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, recommendation.recommendedMaxWorkgroups);
        }
        if (recommendation.shouldSplitDispatches) {
            forceChunkedDispatch = true;
        }
        
        // Check GPU health before proceeding
        if (!timeoutDetector->isGPUHealthy()) {
            std::cerr << "EntityComputeNode: GPU not healthy, reducing workload significantly" << std::endl;
            maxWorkgroupsPerDispatch = std::min(maxWorkgroupsPerDispatch, 512u);
        }
    }
    
    const uint32_t totalWorkgroups = dispatch.groupCountX;
    
    // Debug info (every 5 seconds)
    static int debugCounter = 0;
    if (debugCounter++ % 300 == 0) {
        std::cout << "EntityComputeNode: " << entityCount << " entities → " 
                  << totalWorkgroups << " workgroups (frame " << frameCount << ")" << std::endl;
    }
    
    // Check if we're exceeding device limits
    if (totalWorkgroups > 65535) {
        std::cerr << "ERROR: Workgroup count " << totalWorkgroups << " exceeds Vulkan limit of 65535!" << std::endl;
        return;
    }
    
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
    
    // Debug: Show chunking decision
    std::cout << "EntityComputeNode: totalWorkgroups=" << totalWorkgroups 
              << " maxWorkgroupsPerDispatch=" << maxWorkgroupsPerDispatch 
              << " forceChunkedDispatch=" << forceChunkedDispatch << std::endl;
    
    if (totalWorkgroups <= maxWorkgroupsPerDispatch && !forceChunkedDispatch) {
        // Small dispatch - execute with timeout monitoring
        std::cout << "EntityComputeNode: Using SINGLE dispatch" << std::endl;
        if (timeoutDetector) {
            timeoutDetector->beginComputeDispatch("EntityMovement", totalWorkgroups);
        }
        
        context->getLoader().vkCmdPushConstants(
            commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ComputePushConstants), &pushConstants);
        context->getLoader().vkCmdDispatch(commandBuffer, totalWorkgroups, 1, 1);
        
        if (timeoutDetector) {
            timeoutDetector->endComputeDispatch();
        }
        
        // CRITICAL: Add memory barrier to ensure compute writes are visible to graphics
        VkMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        
        context->getLoader().vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        
        if (debugCounter % 300 == 0) {
            std::cout << "EntityComputeNode: Single dispatch " << totalWorkgroups 
                      << " workgroups for " << entityCount << " entities" << std::endl;
        }
    } else {
        // Large dispatch - split into chunks with memory barriers
        std::cout << "EntityComputeNode: Using CHUNKED dispatch" << std::endl;
        uint32_t processedWorkgroups = 0;
        uint32_t chunkCount = 0;
        
        while (processedWorkgroups < totalWorkgroups) {
            uint32_t currentChunkSize = std::min(maxWorkgroupsPerDispatch, totalWorkgroups - processedWorkgroups);
            uint32_t baseEntityOffset = processedWorkgroups * 64;
            uint32_t remainingEntities = entityCount - baseEntityOffset;
            
            // Skip if no entities left to process
            if (remainingEntities == 0) break;
            
            // Monitor each chunk dispatch
            if (timeoutDetector) {
                std::string chunkName = "EntityMovement_Chunk" + std::to_string(chunkCount);
                timeoutDetector->beginComputeDispatch(chunkName.c_str(), currentChunkSize);
            }
            
            // Update push constants for this chunk
            ComputePushConstants chunkPushConstants = pushConstants;
            chunkPushConstants.entityOffset = baseEntityOffset; // Shader adds this to gl_GlobalInvocationID.x
            
            std::cout << "  Chunk " << chunkCount << ": " << currentChunkSize 
                      << " workgroups, entityOffset=" << baseEntityOffset << std::endl;
            
            // Push constants for this chunk
            context->getLoader().vkCmdPushConstants(
                commandBuffer, dispatch.layout, VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(ComputePushConstants), &chunkPushConstants);
            
            // Dispatch this chunk
            context->getLoader().vkCmdDispatch(commandBuffer, currentChunkSize, 1, 1);
            
            if (timeoutDetector) {
                timeoutDetector->endComputeDispatch();
            }
            
            // Memory barrier between chunks to ensure data consistency
            if (processedWorkgroups + currentChunkSize < totalWorkgroups) {
                VkMemoryBarrier memoryBarrier{};
                memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                
                context->getLoader().vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
            }
            
            processedWorkgroups += currentChunkSize;
            chunkCount++;
        }
        
        // CRITICAL: Add memory barrier after all chunks to ensure compute writes are visible to graphics
        VkMemoryBarrier finalMemoryBarrier{};
        finalMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        finalMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        finalMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        
        context->getLoader().vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 1, &finalMemoryBarrier, 0, nullptr, 0, nullptr);
        
        if (debugCounter % 300 == 0) {
            std::cout << "EntityComputeNode: Split dispatch into " << chunkCount 
                      << " chunks (" << maxWorkgroupsPerDispatch << " workgroups each) for " 
                      << entityCount << " entities" << std::endl;
            
            // Display timeout detector stats if available
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

void EntityComputeNode::updateFrameData(float time, float deltaTime, uint32_t frameCounter) {
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
    pushConstants.frame = frameCounter;
}

