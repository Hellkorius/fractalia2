#include "render_frame_director.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_swapchain.h"
#include "../pipelines/pipeline_system_manager.h"
#include "../core/vulkan_sync.h"
#include "../resources/resource_context.h"
#include "../nodes/entity_compute_node.h"
#include "../nodes/entity_graphics_node.h"
#include "../nodes/swapchain_present_node.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../../ecs/movement_command_system.h"
#include <iostream>

RenderFrameDirector::RenderFrameDirector() {
}

RenderFrameDirector::~RenderFrameDirector() {
    cleanup();
}

bool RenderFrameDirector::initialize(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    PipelineSystemManager* pipelineSystem,
    VulkanSync* sync,
    ResourceContext* resourceContext,
    GPUEntityManager* gpuEntityManager,
    MovementCommandProcessor* movementCommandProcessor,
    FrameGraph* frameGraph
) {
    this->context = context;
    this->swapchain = swapchain;
    this->pipelineSystem = pipelineSystem;
    this->sync = sync;
    this->resourceContext = resourceContext;
    this->gpuEntityManager = gpuEntityManager;
    this->movementCommandProcessor = movementCommandProcessor;
    this->frameGraph = frameGraph;

    return true;
}

void RenderFrameDirector::cleanup() {
    // Nothing to cleanup, dependencies are managed externally
}

RenderFrameResult RenderFrameDirector::directFrame(
    uint32_t currentFrame,
    float totalTime,
    float deltaTime,
    uint32_t frameCounter,
    flecs::world* world
) {
    RenderFrameResult result;

    // 1. Process movement commands
    if (movementCommandProcessor) {
        movementCommandProcessor->processCommands();
    }

    // 2. Acquire swapchain image
    if (!acquireSwapchainImage(currentFrame, result.imageIndex)) {
        return result; // Failed to acquire image
    }

    // 3. Setup frame graph
    setupFrameGraph(result.imageIndex);

    // 4. Compile frame graph (don't execute yet)
    if (!compileFrameGraph(currentFrame, totalTime, deltaTime, frameCounter)) {
        return result;
    }

    // 5. Configure frame graph nodes with world reference after swapchain acquisition
    configureFrameGraphNodes(result.imageIndex, world);

    // 6. Execute frame graph
    std::cout << "RenderFrameDirector: About to execute frame graph for frame " << currentFrame << std::endl;
    result.executionResult = frameGraph->execute(currentFrame);
    std::cout << "RenderFrameDirector: Frame graph execution completed successfully" << std::endl;
    result.success = true;

    std::cout << "RenderFrameDirector: directFrame completed successfully" << std::endl;
    return result;
}

void RenderFrameDirector::updateResourceIds(
    FrameGraphTypes::ResourceId entityBufferId,
    FrameGraphTypes::ResourceId positionBufferId,
    FrameGraphTypes::ResourceId currentPositionBufferId,
    FrameGraphTypes::ResourceId targetPositionBufferId
) {
    this->entityBufferId = entityBufferId;
    this->positionBufferId = positionBufferId;
    this->currentPositionBufferId = currentPositionBufferId;
    this->targetPositionBufferId = targetPositionBufferId;
}

bool RenderFrameDirector::acquireSwapchainImage(uint32_t currentFrame, uint32_t& imageIndex) {
    VkResult result = context->getLoader().vkAcquireNextImageKHR(
        context->getDevice(), 
        swapchain->getSwapchain(), 
        UINT64_MAX, 
        sync->getImageAvailableSemaphores()[currentFrame], 
        VK_NULL_HANDLE, 
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Signal that swapchain recreation is needed
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "RenderFrameDirector: Failed to acquire swap chain image" << std::endl;
        return false;
    }

    return true;
}

void RenderFrameDirector::setupFrameGraph(uint32_t imageIndex) {
    // Only reset frame graph if not already compiled to avoid recompilation every frame
    bool needsInitialization = !frameGraphInitialized;
    if (needsInitialization) {
        frameGraph->reset();
        
        // Initialize swapchain image resource ID cache
        swapchainImageIds.resize(swapchain->getImages().size(), 0);
        
        std::cout << "RenderFrameDirector: Initializing frame graph for first time" << std::endl;
    }
    
    // Import current swapchain image only if not already cached
    if (swapchainImageIds[imageIndex] == 0) {
        VkImage swapchainImage = swapchain->getImages()[imageIndex];
        VkImageView swapchainImageView = swapchain->getImageViews()[imageIndex];
        std::string swapchainName = "SwapchainImage_" + std::to_string(imageIndex);
        swapchainImageIds[imageIndex] = frameGraph->importExternalImage(
            swapchainName,
            swapchainImage,
            swapchainImageView,
            swapchain->getImageFormat(),
            swapchain->getExtent()
        );
    }
    
    swapchainImageId = swapchainImageIds[imageIndex];
    
    // Add nodes to frame graph only once during initialization
    if (needsInitialization) {
        computeNodeId = frameGraph->addNode<EntityComputeNode>(
            entityBufferId,
            positionBufferId,
            currentPositionBufferId,
            targetPositionBufferId,
            pipelineSystem->getComputeManager(),
            gpuEntityManager
        );
        
        // ELEGANT SOLUTION: Pass a dynamic swapchain image reference
        // Nodes will resolve the actual resource ID at execution time
        graphicsNodeId = frameGraph->addNode<EntityGraphicsNode>(
            entityBufferId,
            positionBufferId,
            0, // Placeholder - will be resolved dynamically
            pipelineSystem->getGraphicsManager(),
            swapchain,
            resourceContext,
            gpuEntityManager
        );
        
        presentNodeId = frameGraph->addNode<SwapchainPresentNode>(
            0, // Placeholder - will be resolved dynamically  
            swapchain
        );
        
        // Mark as initialized after nodes are added
        frameGraphInitialized = true;
        std::cout << "RenderFrameDirector: Created nodes - Compute:" << computeNodeId 
                  << " Graphics:" << graphicsNodeId << " Present:" << presentNodeId << std::endl;
    }
    
    // Configure nodes with frame-specific data will be done externally
}

void RenderFrameDirector::configureFrameGraphNodes(uint32_t imageIndex, flecs::world* world) {
    // ELEGANT ORCHESTRATION: Configure nodes with current frame's swapchain image
    if (auto* graphicsNode = frameGraph->getNode<EntityGraphicsNode>(graphicsNodeId)) {
        graphicsNode->setImageIndex(imageIndex);
        graphicsNode->setCurrentSwapchainImageId(swapchainImageId); // Dynamic resolution
        graphicsNode->setWorld(world);
    }
    
    if (auto* presentNode = frameGraph->getNode<SwapchainPresentNode>(presentNodeId)) {
        presentNode->setImageIndex(imageIndex);
        presentNode->setCurrentSwapchainImageId(swapchainImageId); // Dynamic resolution
    }
}

void RenderFrameDirector::configureNodes(
    FrameGraphTypes::NodeId graphicsNodeId, 
    FrameGraphTypes::NodeId presentNodeId, 
    uint32_t imageIndex, 
    flecs::world* world
) {
    // Set the correct image index for the current frame and world reference
    if (EntityGraphicsNode* graphicsNode = frameGraph->getNode<EntityGraphicsNode>(graphicsNodeId)) {
        graphicsNode->setImageIndex(imageIndex);
        graphicsNode->setWorld(world);
    }
    if (SwapchainPresentNode* presentNode = frameGraph->getNode<SwapchainPresentNode>(presentNodeId)) {
        presentNode->setImageIndex(imageIndex);
    }
}

void RenderFrameDirector::resetSwapchainCache() {
    // ELEGANT SOLUTION: Swapchain recreation is now seamless
    // 1. Remove old swapchain images from frame graph
    frameGraph->removeSwapchainResources();
    
    // 2. Reset cache for new swapchain size  
    swapchainImageIds.assign(swapchain->getImages().size(), 0);
    
    // 3. CRITICAL FIX FOR SECOND RESIZE CRASH: Recreate command pool to prevent corruption
    // This addresses cumulative command pool corruption that survives first resize but crashes on second
    if (sync && !sync->recreateCommandPool()) {
        std::cerr << "RenderFrameDirector: CRITICAL ERROR - Failed to recreate command pool during swapchain recreation!" << std::endl;
        std::cerr << "  This may cause subsequent frame rendering to fail or crash." << std::endl;
    } else {
        std::cout << "RenderFrameDirector: Successfully recreated command pool during swapchain recreation" << std::endl;
    }
    
    // 4. CRITICAL FIX: Update BOTH graphics AND compute descriptor sets after swapchain recreation
    // This fixes the second window resize crash by ensuring all descriptor sets have valid buffer bindings
    if (gpuEntityManager && resourceContext) {
        VkBuffer entityBuffer = gpuEntityManager->getEntityBuffer();
        VkBuffer positionBuffer = gpuEntityManager->getPositionBuffer();
        
        if (entityBuffer != VK_NULL_HANDLE && positionBuffer != VK_NULL_HANDLE) {
            bool graphicsSuccess = resourceContext->updateDescriptorSetsWithEntityAndPositionBuffers(entityBuffer, positionBuffer);
            bool computeSuccess = gpuEntityManager->recreateComputeDescriptorSets();
            
            if (graphicsSuccess && computeSuccess) {
                std::cout << "RenderFrameDirector: Successfully updated graphics AND compute descriptor sets after swapchain recreation" << std::endl;
            } else {
                std::cerr << "RenderFrameDirector: ERROR - Failed to update descriptor sets after swapchain recreation!" << std::endl;
                std::cerr << "  Graphics descriptor sets: " << (graphicsSuccess ? "SUCCESS" : "FAILED") << std::endl;
                std::cerr << "  Compute descriptor sets: " << (computeSuccess ? "SUCCESS" : "FAILED") << std::endl;
            }
        } else {
            std::cerr << "RenderFrameDirector: WARNING - Invalid entity or position buffer during swapchain recreation" << std::endl;
            std::cerr << "  Entity buffer: " << (entityBuffer != VK_NULL_HANDLE ? "VALID" : "NULL") << std::endl;
            std::cerr << "  Position buffer: " << (positionBuffer != VK_NULL_HANDLE ? "VALID" : "NULL") << std::endl;
        }
    } else {
        std::cerr << "RenderFrameDirector: WARNING - Missing gpuEntityManager or resourceContext during swapchain recreation" << std::endl;
    }
    
    // 5. That's it! Next frame will naturally import new images
    // No forced rebuilds, no stale references, no complexity
}

bool RenderFrameDirector::compileFrameGraph(uint32_t currentFrame, float totalTime, float deltaTime, uint32_t frameCounter) {
    // Compile frame graph only if not already compiled
    if (!frameGraph->isCompiled() && !frameGraph->compile()) {
        std::cerr << "RenderFrameDirector: Failed to compile frame graph" << std::endl;
        return false;
    }
    
    // Optimized command buffer reset - batch reset for this frame
    sync->resetCommandBuffersForFrame(currentFrame);
    
    // Update frame data in nodes (AAA pattern: frame graph manages per-frame data)
    frameGraph->updateFrameData(totalTime, deltaTime, frameCounter, currentFrame);
    
    return true;
}