#include "entity_compute_node.h"
#include "../compute_pipeline_manager.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../vulkan_context.h"
#include "../vulkan_function_loader.h"
#include "../descriptor_layout_manager.h"
#include <iostream>
#include <array>
#include <glm/glm.hpp>

EntityComputeNode::EntityComputeNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId currentPositionBuffer,
    FrameGraphTypes::ResourceId targetPositionBuffer,
    ComputePipelineManager* computeManager,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , currentPositionBufferId(currentPositionBuffer)
  , targetPositionBufferId(targetPositionBuffer)
  , computeManager(computeManager)
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
    // TEMPORARY: Disable compute to test if graphics work without it
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "EntityComputeNode: DISABLED for testing (frame " << frameCount << ")" << std::endl;
    }
    return;
    
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
    // Use the pipeline system's compute layout (matches compute shader bindings)
    auto layoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
    ComputePipelineState pipelineState = ComputePipelinePresets::createEntityMovementState(descriptorLayout);
    
    // Create compute dispatch
    ComputeDispatch dispatch{};
    dispatch.pipeline = computeManager->getPipeline(pipelineState);
    dispatch.layout = computeManager->getPipelineLayout(pipelineState);
    
    if (dispatch.pipeline == VK_NULL_HANDLE || dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << "EntityComputeNode: Failed to get compute pipeline" << std::endl;
        return;
    }
    
    // Set up descriptor sets
    VkDescriptorSet computeDescriptorSet = gpuEntityManager->getComputeDescriptorSet();
    if (computeDescriptorSet != VK_NULL_HANDLE) {
        dispatch.descriptorSets.push_back(computeDescriptorSet);
    }
    
    // Update push constants with current frame data
    pushConstants.entityCount = gpuEntityManager->getEntityCount();
    dispatch.pushConstantData = &pushConstants;
    dispatch.pushConstantSize = sizeof(ComputePushConstants);
    dispatch.pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Calculate optimal dispatch size
    uint32_t entityCount = gpuEntityManager->getEntityCount();
    dispatch.calculateOptimalDispatch(entityCount, glm::uvec3(32, 1, 1));
    
    // Execute compute dispatch using new pipeline manager
    computeManager->dispatch(commandBuffer, dispatch);
    
    // Debug compute dispatch (once per second)
    static int dispatchCounter = 0;
    if (dispatchCounter++ % 60 == 0) {
        std::cout << "EntityComputeNode: Dispatched " << dispatch.groupCountX << " workgroups for " 
                  << pushConstants.entityCount << " entities (frame=" << pushConstants.frame 
                  << ", time=" << pushConstants.time << ") [AAA pipeline]" << std::endl;
    }
}

void EntityComputeNode::updateFrameData(float time, float deltaTime, uint32_t frameCounter) {
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
    pushConstants.frame = frameCounter;
}

