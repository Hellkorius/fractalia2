#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <glm/glm.hpp>
#include <flecs.h>
#include "../core/vulkan_constants.h"
#include "../rendering/frame_graph.h"

// Forward declarations
class VulkanContext;
class VulkanSwapchain;
class VulkanSync;
class ResourceCoordinator;
class GPUEntityManager;
class PipelineSystemManager;
class PresentationSurface;

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
        ResourceCoordinator* resourceCoordinator,
        GPUEntityManager* gpuEntityManager,
        FrameGraph* frameGraph,
        PresentationSurface* presentationSurface
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
    
    // Swapchain recreation support
    void resetSwapchainCache();

private:
    // Dependencies
    VulkanContext* context = nullptr;
    VulkanSwapchain* swapchain = nullptr;
    PipelineSystemManager* pipelineSystem = nullptr;
    VulkanSync* sync = nullptr;
    ResourceCoordinator* resourceCoordinator = nullptr;
    GPUEntityManager* gpuEntityManager = nullptr;
    FrameGraph* frameGraph = nullptr;
    PresentationSurface* presentationSurface = nullptr;

    // Resource IDs
    FrameGraphTypes::ResourceId entityBufferId = 0;
    FrameGraphTypes::ResourceId positionBufferId = 0;
    FrameGraphTypes::ResourceId currentPositionBufferId = 0;
    FrameGraphTypes::ResourceId targetPositionBufferId = 0;
    FrameGraphTypes::ResourceId swapchainImageId = 0;
    FrameGraphTypes::ResourceId shadowDepthTargetId = 0;
    
    // State management
    bool frameGraphInitialized = false;
    std::vector<FrameGraphTypes::ResourceId> swapchainImageIds; // Cached per swapchain image
    
    // Global frame counter for compute shader consistency
    std::atomic<uint32_t> globalFrameCounter_{0};
    
    // Node IDs for configuration
    FrameGraphTypes::NodeId computeNodeId = 0;
    FrameGraphTypes::NodeId physicsNodeId = 0;
    FrameGraphTypes::NodeId graphicsNodeId = 0;
    FrameGraphTypes::NodeId presentNodeId = 0;
    FrameGraphTypes::NodeId shadowNodeId = 0;
    FrameGraphTypes::NodeId sunParticleComputeNodeId = 0;
    FrameGraphTypes::NodeId sunSystemNodeId = 0;
    
    // Shadow configuration
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.3f, -0.8f, 0.5f)); // Interesting angle
    float shadowDistance = 1000.0f;
    uint32_t cascadeCount = 3;
    
    // Scene configuration for sun system
    glm::vec4 sceneCenter = glm::vec4(0.0f, 0.0f, 0.0f, 150.0f); // Scene bounds for sun particles

    // Helper methods
    void setupFrameGraph(uint32_t imageIndex);
    void configureNodes(FrameGraphTypes::NodeId graphicsNodeId, FrameGraphTypes::NodeId presentNodeId, uint32_t imageIndex, flecs::world* world);
    bool compileFrameGraph(uint32_t currentFrame, float totalTime, float deltaTime, uint32_t frameCounter);
};