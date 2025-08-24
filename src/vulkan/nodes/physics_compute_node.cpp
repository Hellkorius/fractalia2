#include "physics_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../core/vulkan_constants.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../pipelines/compute_pipeline_types.h"
#include <algorithm>

PhysicsComputeNode::PhysicsComputeNode(
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
    "PhysicsComputeNode"
) {
    // All validation and initialization is handled by BaseComputeNode
}

std::vector<ResourceDependency> PhysicsComputeNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},
        {currentPositionBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},
    };
}

std::vector<ResourceDependency> PhysicsComputeNode::getOutputs() const {
    return {
        {positionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
        {currentPositionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
    };
}

void PhysicsComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph, float time, float deltaTime) {
    // Delegate to the base class template method
    executeComputeNode(commandBuffer, frameGraph, time, deltaTime, "Physics");
}

// Virtual method implementations specific to physics computation
BaseComputeNode::DispatchParams PhysicsComputeNode::calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) {
    // Calculate workgroups needed for both spatial map clearing and entity processing
    const uint32_t SPATIAL_MAP_SIZE = 16384; // 32x32x16 (3D spatial grid)
    const uint32_t spatialClearWorkgroups = (SPATIAL_MAP_SIZE + THREADS_PER_WORKGROUP - 1) / THREADS_PER_WORKGROUP;
    const uint32_t entityWorkgroups = (entityCount + THREADS_PER_WORKGROUP - 1) / THREADS_PER_WORKGROUP;
    
    // Use maximum of both requirements
    const uint32_t totalWorkgroups = std::max(spatialClearWorkgroups, entityWorkgroups);
    
    return {
        totalWorkgroups,
        maxWorkgroups,
        totalWorkgroups > maxWorkgroups || forceChunking
    };
}

ComputePipelineState PhysicsComputeNode::createPipelineState(VkDescriptorSetLayout descriptorLayout) {
    return ComputePipelinePresets::createPhysicsState(descriptorLayout);
}

void PhysicsComputeNode::setupPushConstants(float time, float deltaTime, uint32_t entityCount, uint32_t frameCounter) {
    // Update push constants with timing data and frame counter
    pushConstants.time = time;
    pushConstants.deltaTime = deltaTime;
    pushConstants.frame = frameCounter;
}