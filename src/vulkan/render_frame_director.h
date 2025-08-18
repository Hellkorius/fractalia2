#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <flecs.h>
#include "vulkan_constants.h"
#include "frame_graph.h"

// Forward declarations
class VulkanContext;
class VulkanSwapchain;
class VulkanSync;
class ResourceContext;
class GPUEntityManager;
class MovementCommandProcessor;
class PipelineSystemManager;

struct RenderFrameResult {
    bool success = false;
    uint32_t imageIndex = 0;
    FrameGraph::ExecutionResult executionResult;
};

class RenderFrameDirector {
public:
    RenderFrameDirector();
    ~RenderFrameDirector();

    bool initialize(
        VulkanContext* context,
        VulkanSwapchain* swapchain,
        PipelineSystemManager* pipelineSystem,
        VulkanSync* sync,
        ResourceContext* resourceContext,
        GPUEntityManager* gpuEntityManager,
        MovementCommandProcessor* movementCommandProcessor,
        FrameGraph* frameGraph
    );

    void cleanup();

    // Main frame direction
    RenderFrameResult directFrame(
        uint32_t currentFrame,
        float totalTime,
        float deltaTime,
        uint32_t frameCounter,
        flecs::world* world
    );

    // Resource management
    void updateResourceIds(
        FrameGraphTypes::ResourceId entityBufferId,
        FrameGraphTypes::ResourceId positionBufferId,
        FrameGraphTypes::ResourceId currentPositionBufferId,
        FrameGraphTypes::ResourceId targetPositionBufferId
    );

    // Node configuration after setup
    void configureFrameGraphNodes(uint32_t imageIndex, flecs::world* world);

private:
    // Dependencies
    VulkanContext* context = nullptr;
    VulkanSwapchain* swapchain = nullptr;
    PipelineSystemManager* pipelineSystem = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    GPUEntityManager* gpuEntityManager = nullptr;
    MovementCommandProcessor* movementCommandProcessor = nullptr;
    FrameGraph* frameGraph = nullptr;

    // Resource IDs
    FrameGraphTypes::ResourceId entityBufferId = 0;
    FrameGraphTypes::ResourceId positionBufferId = 0;
    FrameGraphTypes::ResourceId currentPositionBufferId = 0;
    FrameGraphTypes::ResourceId targetPositionBufferId = 0;
    FrameGraphTypes::ResourceId swapchainImageId = 0;
    
    // State management
    bool frameGraphInitialized = false;
    std::vector<FrameGraphTypes::ResourceId> swapchainImageIds; // Cached per swapchain image
    
    // Node IDs for configuration
    FrameGraphTypes::NodeId computeNodeId = 0;
    FrameGraphTypes::NodeId graphicsNodeId = 0;
    FrameGraphTypes::NodeId presentNodeId = 0;

    // Helper methods
    bool acquireSwapchainImage(uint32_t currentFrame, uint32_t& imageIndex);
    void setupFrameGraph(uint32_t imageIndex);
    void configureNodes(FrameGraphTypes::NodeId graphicsNodeId, FrameGraphTypes::NodeId presentNodeId, uint32_t imageIndex, flecs::world* world);
    bool compileFrameGraph(uint32_t currentFrame, float totalTime, float deltaTime, uint32_t frameCounter);
};