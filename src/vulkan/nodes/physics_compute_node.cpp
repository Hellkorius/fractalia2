#include "physics_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../core/vulkan_constants.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../pipelines/compute_pipeline_types.h"
#include <algorithm>

PhysicsComputeNode::PhysicsComputeNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId modelMatrixBuffer,
    ComputePipelineManager* computeManager,
    GPUEntityManager* gpuEntityManager,
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector
) : BaseComputeNode(
    entityBuffer,
    modelMatrixBuffer,  // Physics now writes to model matrices
    modelMatrixBuffer,  // Use same buffer for current/target (no ping-pong needed)
    modelMatrixBuffer,  // Physics updates model matrices directly
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
        {positionBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader},  // Model matrix buffer
    };
}

std::vector<ResourceDependency> PhysicsComputeNode::getOutputs() const {
    return {
        {positionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},  // Model matrix buffer
    };
}

void PhysicsComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph, float time, float deltaTime) {
    // Delegate to the base class template method
    executeComputeNode(commandBuffer, frameGraph, time, deltaTime, "Physics");
}

// Virtual method implementations specific to physics computation
BaseComputeNode::DispatchParams PhysicsComputeNode::calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) {
    // Calculate workgroups needed for both spatial map clearing and entity processing
    const uint32_t SPATIAL_MAP_SIZE = 16384; // 32x32x16 (first-principles 3D spatial grid)
    const uint32_t spatialClearWorkgroups = (SPATIAL_MAP_SIZE + THREADS_PER_WORKGROUP - 1) / THREADS_PER_WORKGROUP;
    const uint32_t entityWorkgroups = (entityCount + THREADS_PER_WORKGROUP - 1) / THREADS_PER_WORKGROUP;
    
    // Use maximum of both requirements
    const uint32_t totalWorkgroups = std::max(spatialClearWorkgroups, entityWorkgroups);
    
    // Disable forced chunking for modern GPUs unless truly necessary
    const bool shouldChunk = (totalWorkgroups > maxWorkgroups) && (maxWorkgroups < 2048);
    
    return {
        totalWorkgroups,
        maxWorkgroups,
        shouldChunk || forceChunking
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
    
    // Set gravity and collision parameters
    pushConstants.gravityStrength = 15.0f;  // Gravity acceleration (tuned for the engine scale)
    pushConstants.restitution = 0.5f;       // Moderate bounce (50% energy retention)
    pushConstants.friction = 0.8f;          // Surface friction (20% velocity loss)
}