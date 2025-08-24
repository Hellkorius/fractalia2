#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"
#include "../rendering/frame_graph_debug.h"
#include "../rendering/frame_graph_types.h"
#include "../core/vulkan_constants.h"
#include "../pipelines/compute_pipeline_types.h"
#include "../pipelines/descriptor_layout_manager.h"
#include <memory>

// Forward declarations
class ComputePipelineManager;
class GPUEntityManager;
class GPUTimeoutDetector;
class VulkanContext;
struct ComputeDispatch;

/**
 * Base class for compute nodes that eliminates massive code duplication
 * between EntityComputeNode and PhysicsComputeNode.
 * 
 * Shared functionality includes:
 * - Constructor validation and member initialization
 * - Dispatch parameter calculation and validation
 * - Adaptive workload management with timeout detection
 * - Chunked dispatch execution with memory barriers
 * - Pipeline binding and descriptor set management
 */
class BaseComputeNode : public FrameGraphNode {
protected:
    struct DispatchParams {
        uint32_t totalWorkgroups;
        uint32_t maxWorkgroupsPerChunk;
        bool useChunking;
    };

    BaseComputeNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId currentPositionBuffer,
        FrameGraphTypes::ResourceId targetPositionBuffer,
        ComputePipelineManager* computeManager,
        GPUEntityManager* gpuEntityManager,
        std::shared_ptr<GPUTimeoutDetector> timeoutDetector,
        const char* nodeTypeName
    );

    // Queue requirements - all compute nodes need compute queue
    bool needsComputeQueue() const override final { return true; }
    bool needsGraphicsQueue() const override final { return false; }

    // Shared execution logic - template method pattern
    void executeComputeNode(
        VkCommandBuffer commandBuffer, 
        const FrameGraph& frameGraph, 
        float time, 
        float deltaTime,
        const char* pipelinePresetName
    );

    // Virtual methods for node-specific behavior
    virtual DispatchParams calculateDispatchParams(uint32_t entityCount, uint32_t maxWorkgroups, bool forceChunking) = 0;
    virtual ComputePipelineState createPipelineState(VkDescriptorSetLayout descriptorLayout) = 0;
    virtual void setupPushConstants(float time, float deltaTime, uint32_t entityCount, uint32_t frameCounter) = 0;
    virtual const char* getNodeName() const = 0;
    virtual const char* getDispatchBaseName() const = 0;

    // Dependency validation
    void onFirstUse(const FrameGraph& frameGraph) override;

    // Shared resource IDs
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId currentPositionBufferId;
    FrameGraphTypes::ResourceId targetPositionBufferId;

    // Shared dependencies
    ComputePipelineManager* computeManager;
    GPUEntityManager* gpuEntityManager;
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector;

    // Adaptive dispatch parameters - optimized defaults
    uint32_t adaptiveMaxWorkgroups = MAX_WORKGROUPS_PER_CHUNK;
    bool forceChunkedDispatch = false;  // Disable forced chunking on modern GPUs

    // Push constants
    NodePushConstants pushConstants{};

private:
    // Shared implementation methods
    void executeChunkedDispatch(
        VkCommandBuffer commandBuffer, 
        const VulkanContext* context, 
        const ComputeDispatch& dispatch,
        uint32_t totalWorkgroups,
        uint32_t maxWorkgroupsPerChunk,
        uint32_t entityCount
    );

    void executeSingleDispatch(
        VkCommandBuffer commandBuffer,
        const VulkanContext* context,
        const ComputeDispatch& dispatch,
        uint32_t totalWorkgroups
    );

    void createMemoryBarrier(VkCommandBuffer commandBuffer, const VulkanContext* context);

    // Shared validation and setup
    bool validateDependencies() const;
    bool validateDispatchLimits(uint32_t totalWorkgroups) const;
    void applyAdaptiveWorkloadManagement(uint32_t& maxWorkgroupsPerDispatch, bool& shouldForceChunking) const;

    // Node type name for error messages and logging
    const char* nodeTypeName;

    // Debug counter for throttled logging
    mutable FrameGraphDebug::DebugCounter debugCounter{};
};