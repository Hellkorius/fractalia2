#include "entity_compute_node.h"
#include "../vulkan_pipeline.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../vulkan_context.h"
#include "../vulkan_function_loader.h"
#include <iostream>
#include <array>

EntityComputeNode::EntityComputeNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId currentPositionBuffer,
    FrameGraphTypes::ResourceId targetPositionBuffer,
    VulkanPipeline* pipeline,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , currentPositionBufferId(currentPositionBuffer)
  , targetPositionBufferId(targetPositionBuffer)
  , pipeline(pipeline)
  , gpuEntityManager(gpuEntityManager) {
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
    if (!pipeline || !gpuEntityManager) {
        std::cerr << "EntityComputeNode: Missing dependencies" << std::endl;
        return;
    }
    
    // Only dispatch if we have entities to process
    if (gpuEntityManager->getEntityCount() == 0) {
        return;
    }
    
    // Get Vulkan context from frame graph
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << "EntityComputeNode: Missing Vulkan context" << std::endl;
        return;
    }
    
    // Bind compute pipeline
    context->getLoader().vkCmdBindPipeline(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->getComputePipeline()
    );
    
    // Bind compute descriptor set
    VkDescriptorSet computeDescriptorSet = gpuEntityManager->getCurrentComputeDescriptorSet();
    context->getLoader().vkCmdBindDescriptorSets(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->getComputePipelineLayout(), 
        0, 1, &computeDescriptorSet, 0, nullptr
    );
    
    // Update push constants with current frame data
    pushConstants.entityCount = gpuEntityManager->getEntityCount();
    
    context->getLoader().vkCmdPushConstants(
        commandBuffer,
        pipeline->getComputePipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(ComputePushConstants),
        &pushConstants
    );
    
    // Dispatch compute shader (32 threads per workgroup for optimal performance)
    uint32_t numWorkgroups = (pushConstants.entityCount + 31) / 32;
    context->getLoader().vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);
    
    std::cout << "EntityComputeNode: Dispatched compute for " 
              << pushConstants.entityCount << " entities (" 
              << numWorkgroups << " workgroups)" << std::endl;
}

void EntityComputeNode::updateFrameData(float time, float deltaTime, uint32_t frameCounter) {
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
    pushConstants.frame = frameCounter;
}

