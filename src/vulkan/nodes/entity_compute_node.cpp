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
    VulkanPipeline* pipeline,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , pipeline(pipeline)
  , gpuEntityManager(gpuEntityManager) {
}

std::vector<ResourceDependency> EntityComputeNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::Read, PipelineStage::ComputeShader},
    };
}

std::vector<ResourceDependency> EntityComputeNode::getOutputs() const {
    return {
        {positionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
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
    
    // Insert memory barriers to ensure compute writes complete before graphics reads
    insertBufferBarriers(commandBuffer, context);
    
    std::cout << "EntityComputeNode: Dispatched compute for " 
              << pushConstants.entityCount << " entities (" 
              << numWorkgroups << " workgroups)" << std::endl;
}

void EntityComputeNode::updateFrameData(float time, float deltaTime, uint32_t frameCounter) {
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
    pushConstants.frame = frameCounter;
}

void EntityComputeNode::insertBufferBarriers(VkCommandBuffer commandBuffer, const VulkanContext* context) {
    if (!gpuEntityManager || gpuEntityManager->getEntityCount() == 0) {
        return;
    }
    
    // Buffer memory barriers to ensure proper synchronization between compute and graphics stages
    std::array<VkBufferMemoryBarrier, 4> bufferBarriers{};
    
    // GPUEntity buffer barrier (compute read/write -> vertex shader instance data read)
    bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[0].buffer = gpuEntityManager->getCurrentEntityBuffer();
    bufferBarriers[0].offset = 0;
    bufferBarriers[0].size = VK_WHOLE_SIZE;
    
    // Position buffer barrier (compute write -> vertex shader storage buffer read)
    bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[1].buffer = gpuEntityManager->getCurrentPositionBuffer();
    bufferBarriers[1].offset = 0;
    bufferBarriers[1].size = VK_WHOLE_SIZE;
    
    // Current position buffer barrier (compute read/write -> potential next frame access)
    bufferBarriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[2].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[2].buffer = gpuEntityManager->getCurrentPositionStorageBuffer();
    bufferBarriers[2].offset = 0;
    bufferBarriers[2].size = VK_WHOLE_SIZE;
    
    // Target position buffer barrier (compute read/write -> potential next frame access)
    bufferBarriers[3].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[3].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[3].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[3].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[3].buffer = gpuEntityManager->getTargetPositionStorageBuffer();
    bufferBarriers[3].offset = 0;
    bufferBarriers[3].size = VK_WHOLE_SIZE;
    
    context->getLoader().vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 
        0, nullptr,                                        // No general memory barriers
        static_cast<uint32_t>(bufferBarriers.size()), 
        bufferBarriers.data(),                             // Buffer barriers
        0, nullptr                                         // No image barriers
    );
}