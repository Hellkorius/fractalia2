#include "entity_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../core/vulkan_constants.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../pipelines/compute_pipeline_types.h"

EntityComputeNode::EntityComputeNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId currentPositionBuffer,
    FrameGraphTypes::ResourceId targetPositionBuffer,
    ComputePipelineManager* computeManager,
    GPUEntityManager* gpuEntityManager,
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector
) : BaseComputeNode(
    entityBuffer,
    positionBuffer, 
    currentPositionBuffer,
    targetPositionBuffer,
    computeManager,
    gpuEntityManager,
    timeoutDetector,
    "EntityComputeNode"
) {
    // All validation and initialization is handled by BaseComputeNode
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

void EntityComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph, float time, float deltaTime) {
    // Delegate to the base class template method
    executeComputeNode(commandBuffer, frameGraph, time, deltaTime, "Movement");
}

// Virtual method implementations specific to entity movement
BaseComputeNode::DispatchParams EntityComputeNode::calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) {
    const uint32_t totalWorkgroups = (entityCount + THREADS_PER_WORKGROUP - 1) / THREADS_PER_WORKGROUP;
    return {
        totalWorkgroups,
        maxWorkgroups,
        totalWorkgroups > maxWorkgroups || forceChunking
    };
}

ComputePipelineState EntityComputeNode::createPipelineState(VkDescriptorSetLayout descriptorLayout) {
    return ComputePipelinePresets::createEntityMovementState(descriptorLayout);
}

void EntityComputeNode::setupPushConstants(float time, float deltaTime, uint32_t entityCount, uint32_t frameCounter) {
    // Set frame counter from FrameGraph for compute shader consistency
    pushConstants.frame = frameCounter;
    // EntityComputeNode doesn't need time/deltaTime in push constants - handled by compute shader
}