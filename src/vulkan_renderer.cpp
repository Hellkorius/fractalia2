#include "vulkan_renderer.h"
#include "vulkan/vulkan_function_loader.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_sync.h"
#include "vulkan/resource_context.h"
#include "vulkan/frame_graph.h"
#include "vulkan/nodes/entity_compute_node.h"
#include "vulkan/nodes/entity_graphics_node.h"
#include "vulkan/nodes/swapchain_present_node.h"
#include "ecs/gpu_entity_manager.h"
#include "ecs/component.h"
#include "ecs/systems/camera_system.h"
#include "ecs/camera_component.h"
#include "ecs/movement_command_system.h"
#include "ecs/systems/simple_control_system.h"
#include <iostream>
#include <array>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Static member definition
float VulkanRenderer::clampedDeltaTime = 0.0f;

VulkanRenderer::VulkanRenderer() {
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::initialize(SDL_Window* window) {
    this->window = window;
    
    
    context = std::make_unique<VulkanContext>();
    if (!context->initialize(window)) {
        std::cerr << "Failed to initialize Vulkan context" << std::endl;
        return false;
    }
    
    swapchain = std::make_unique<VulkanSwapchain>();
    if (!swapchain->initialize(*context, window)) {
        std::cerr << "Failed to initialize Vulkan swapchain" << std::endl;
        return false;
    }
    
    pipeline = std::make_unique<VulkanPipeline>();
    if (!pipeline->initialize(*context, swapchain->getImageFormat())) {
        std::cerr << "Failed to initialize Vulkan pipeline" << std::endl;
        return false;
    }
    
    if (!swapchain->createFramebuffers(pipeline->getRenderPass())) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return false;
    }
    
    sync = std::make_unique<VulkanSync>();
    if (!sync->initialize(*context)) {
        std::cerr << "Failed to initialize Vulkan sync" << std::endl;
        return false;
    }
    
    resourceContext = std::make_unique<ResourceContext>();
    if (!resourceContext->initialize(*context, sync->getCommandPool())) {
        std::cerr << "Failed to initialize Resource context" << std::endl;
        return false;
    }
    
    if (!resourceContext->createUniformBuffers()) {
        std::cerr << "Failed to create uniform buffers" << std::endl;
        return false;
    }
    
    if (!resourceContext->createTriangleBuffers()) {
        std::cerr << "Failed to create triangle buffers" << std::endl;
        return false;
    }
    
    if (!resourceContext->createGraphicsDescriptorPool(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }
    
    if (!resourceContext->createGraphicsDescriptorSets(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor sets" << std::endl;
        return false;
    }
    
    // Initialize GPU entity manager after sync and resource context are created
    gpuEntityManager = std::make_unique<GPUEntityManager>();
    if (!gpuEntityManager->initialize(*context, sync.get(), resourceContext.get(), pipeline->getComputeDescriptorSetLayout())) {
        std::cerr << "Failed to initialize GPU entity manager" << std::endl;
        return false;
    }
    
    // Update graphics descriptor sets with position buffer for compute-based rendering
    if (!resourceContext->updateDescriptorSetsWithPositionBuffer(gpuEntityManager->getCurrentPositionBuffer())) {
        std::cerr << "Failed to update descriptor sets with position buffer" << std::endl;
        return false;
    }
    std::cout << "Graphics descriptor sets updated with position buffer" << std::endl;
    
    // Initialize movement command processor
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    std::cout << "Movement command processor initialized" << std::endl;
    
    // Initialize frame graph system
    if (!initializeFrameGraph()) {
        std::cerr << "Failed to initialize frame graph" << std::endl;
        return false;
    }
    
    // Initialize per-frame fences
    if (!frameFences.initialize(*context)) {
        std::cerr << "Failed to initialize frame fences" << std::endl;
        return false;
    }
    
    
    initialized = true;
    return true;
}

void VulkanRenderer::cleanup() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        context->getLoader().vkDeviceWaitIdle(context->getDevice());
        
        // Clean up per-frame fences
        frameFences.cleanup();
    }
    
    // Clean up frame graph system
    cleanupFrameGraph();
    
    movementCommandProcessor.reset();
    gpuEntityManager.reset();
    resourceContext.reset();
    sync.reset();
    pipeline.reset();
    swapchain.reset();
    context.reset();
    
    initialized = false;
}

void VulkanRenderer::drawFrame() {
    // Phase 3: Cutover to frame graph system
    drawFrameWithFrameGraph();
}

bool VulkanRenderer::recreateSwapChain() {
    // Prevent concurrent recreation attempts
    if (recreationInProgress) {
        std::cout << "VulkanRenderer: Recreation already in progress, skipping..." << std::endl;
        return true;
    }
    recreationInProgress = true;
    
    // Wait for all frame fences to be signaled instead of vkDeviceWaitIdle
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (frameFences.isComputeInUse(i)) {
            VkFence computeFence = frameFences.getComputeFence(i);
            VkResult result = waitForFenceRobust(computeFence, "compute");
            if (result == VK_ERROR_DEVICE_LOST) {
                recreationInProgress = false;
                return false;
            }
            frameFences.setComputeInUse(i, false);
        }
        if (frameFences.isGraphicsInUse(i)) {
            VkFence graphicsFence = frameFences.getGraphicsFence(i);
            VkResult result = waitForFenceRobust(graphicsFence, "graphics");
            if (result == VK_ERROR_DEVICE_LOST) {
                recreationInProgress = false;
                return false;
            }
            frameFences.setGraphicsInUse(i, false);
        }
    }
    
    if (!swapchain->recreate(pipeline->getRenderPass())) {
        recreationInProgress = false;
        return false;
    }
    
    if (!pipeline->recreate(swapchain->getImageFormat())) {
        recreationInProgress = false;
        return false;
    }
    
    if (!swapchain->createFramebuffers(pipeline->getRenderPass())) {
        recreationInProgress = false;
        return false;
    }
    
    // Update GPU entity manager with new compute descriptor set layout from recreated pipeline
    if (gpuEntityManager) {
        if (!gpuEntityManager->recreateComputeDescriptorResources(pipeline->getComputeDescriptorSetLayout())) {
            std::cerr << "Failed to recreate GPU entity manager descriptor sets!" << std::endl;
            recreationInProgress = false;
            return false;
        }
        
        // Update graphics descriptor sets with position buffer after recreation
        if (!resourceContext->updateDescriptorSetsWithPositionBuffer(gpuEntityManager->getCurrentPositionBuffer())) {
            std::cerr << "Failed to update graphics descriptor sets with position buffer!" << std::endl;
            recreationInProgress = false;
            return false;
        }
    }

    recreationInProgress = false;
    return true;
}



void VulkanRenderer::uploadPendingGPUEntities() {
    if (gpuEntityManager) {
        gpuEntityManager->flushStagingBuffer();
    }
}








bool VulkanRenderer::validateEntityCapacity(uint32_t entityCount, const char* source) const {
#ifdef _DEBUG
    assert(gpuEntityManager && "GPU entity manager must exist");
    assert(entityCount <= gpuEntityManager->getMaxEntities() && "Entity count must not exceed capacity");
#else
    (void)entityCount;
    (void)source;
#endif
    return true;
}

bool VulkanRenderer::testBufferOverflowProtection() const {
#ifdef _DEBUG
    assert(initialized && "Renderer must be initialized");
    assert(gpuEntityManager && "GPU entity manager must be available");
    
    uint32_t gpuEntityCount = gpuEntityManager->getEntityCount();
    uint32_t maxGpuEntities = gpuEntityManager->getMaxEntities();
    assert(gpuEntityCount <= maxGpuEntities && "GPU entities must be within buffer limits");
    
    std::cout << "GPU entities: " << gpuEntityCount << "/" << maxGpuEntities << std::endl;
#endif
    return true;
}

void VulkanRenderer::updateAspectRatio(int windowWidth, int windowHeight) {
    if (world != nullptr) {
        CameraManager::updateAspectRatio(*world, windowWidth, windowHeight);
    }
}

// FrameFences implementation
bool VulkanRenderer::FrameFences::initialize(const VulkanContext& context) {
    this->context = &context;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (context.getLoader().vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create compute fence for frame " << i << std::endl;
            cleanup();
            return false;
        }
        
        if (context.getLoader().vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &graphicsFences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics fence for frame " << i << std::endl;
            // Clean up compute fence just created
            context.getLoader().vkDestroyFence(context.getDevice(), computeFences[i], nullptr);
            computeFences[i] = VK_NULL_HANDLE;
            cleanup();
            return false;
        }
        
        computeInUse[i] = false;
        graphicsInUse[i] = false;
    }
    
    initialized = true;
    return true;
}

void VulkanRenderer::FrameFences::cleanup() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (computeFences[i] != VK_NULL_HANDLE) {
                context->getLoader().vkDestroyFence(context->getDevice(), computeFences[i], nullptr);
                computeFences[i] = VK_NULL_HANDLE;
            }
            if (graphicsFences[i] != VK_NULL_HANDLE) {
                context->getLoader().vkDestroyFence(context->getDevice(), graphicsFences[i], nullptr);
                graphicsFences[i] = VK_NULL_HANDLE;
            }
            computeInUse[i] = false;
            graphicsInUse[i] = false;
        }
    }
    initialized = false;
    context = nullptr;
}

VkResult VulkanRenderer::waitForFenceRobust(VkFence fence, const char* fenceName) {
    // First try immediate check
    VkResult result = context->getLoader().vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, FENCE_TIMEOUT_IMMEDIATE);
    if (result == VK_TIMEOUT) {
        // If not ready immediately, wait up to one frame time
        result = context->getLoader().vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, FENCE_TIMEOUT_FRAME);
        if (result == VK_TIMEOUT) {
            // Proper timeout handling: reset fence and maintain synchronization
            std::cerr << "Warning: " << fenceName << " fence timeout, forcing synchronization" << std::endl;
            
            // Force wait with longer timeout to prevent desync
            result = context->getLoader().vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) {
                std::cerr << "Critical: Failed to synchronize " << fenceName << " pipeline: " << result << std::endl;
                // Reset state to prevent further issues
                context->getLoader().vkResetFences(context->getDevice(), 1, &fence);
                return VK_ERROR_DEVICE_LOST; // Signal critical error
            }
        }
    }
    
    return result;
}

bool VulkanRenderer::initializeFrameGraph() {
    // Create frame graph instance
    frameGraph = std::make_unique<FrameGraph>();
    if (!frameGraph->initialize(*context, sync.get())) {
        std::cerr << "Failed to initialize frame graph" << std::endl;
        return false;
    }
    
    // Import external resources that are managed outside the frame graph
    entityBufferId = frameGraph->importExternalBuffer(
        "EntityBuffer", 
        gpuEntityManager->getCurrentEntityBuffer(), 
        gpuEntityManager->getMaxEntities() * sizeof(GPUEntity),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );
    
    positionBufferId = frameGraph->importExternalBuffer(
        "PositionBuffer",
        gpuEntityManager->getCurrentPositionBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(glm::vec4),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    
    currentPositionBufferId = frameGraph->importExternalBuffer(
        "CurrentPositionBuffer",
        gpuEntityManager->getCurrentPositionStorageBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(glm::vec4),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    
    targetPositionBufferId = frameGraph->importExternalBuffer(
        "TargetPositionBuffer",
        gpuEntityManager->getTargetPositionStorageBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(glm::vec4),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    
    // Note: Swapchain images will be imported per-frame since they change
    // Node creation will be done per-frame to handle dynamic swapchain images
    
    std::cout << "Frame graph initialized successfully" << std::endl;
    return true;
}

void VulkanRenderer::cleanupFrameGraph() {
    if (frameGraph) {
        frameGraph->cleanup();
        frameGraph.reset();
    }
    
    computeNode.reset();
    graphicsNode.reset();
    presentNode.reset();
}

void VulkanRenderer::drawFrameWithFrameGraph() {
    // 1. Wait for previous frame fences
    if (frameFences.isComputeInUse(currentFrame)) {
        VkResult computeResult = waitForFenceRobust(frameFences.getComputeFence(currentFrame), "compute");
        if (computeResult == VK_ERROR_DEVICE_LOST) {
            frameFences.setComputeInUse(currentFrame, false);
            return;
        }
        frameFences.setComputeInUse(currentFrame, false);
    }
    
    if (frameFences.isGraphicsInUse(currentFrame)) {
        VkResult graphicsResult = waitForFenceRobust(frameFences.getGraphicsFence(currentFrame), "graphics");
        if (graphicsResult == VK_ERROR_DEVICE_LOST) {
            frameFences.setGraphicsInUse(currentFrame, false);
            return;
        }
        frameFences.setGraphicsInUse(currentFrame, false);
    }

    // 2. Process movement commands
    if (movementCommandProcessor) {
        movementCommandProcessor->processCommands();
    }

    // 3. Acquire swapchain image
    uint32_t imageIndex;
    VkResult result = context->getLoader().vkAcquireNextImageKHR(
        context->getDevice(), 
        swapchain->getSwapchain(), 
        UINT64_MAX, 
        sync->getImageAvailableSemaphores()[currentFrame], 
        VK_NULL_HANDLE, 
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Failed to acquire swap chain image" << std::endl;
        return;
    }

    // 4. Upload pending data
    uploadPendingGPUEntities();

    // 5. Setup frame graph for current frame
    frameGraph->reset();
    
    // Import current swapchain image with unique name per frame
    VkImage swapchainImage = swapchain->getImages()[imageIndex];
    VkImageView swapchainImageView = swapchain->getImageViews()[imageIndex];
    std::string swapchainName = "SwapchainImage_" + std::to_string(imageIndex);
    swapchainImageId = frameGraph->importExternalImage(
        swapchainName,
        swapchainImage,
        swapchainImageView,
        swapchain->getImageFormat(),
        swapchain->getExtent()
    );
    
    // Add nodes to frame graph
    frameGraph->addNode<EntityComputeNode>(
        entityBufferId,
        positionBufferId,
        currentPositionBufferId,
        targetPositionBufferId,
        pipeline.get(),
        gpuEntityManager.get()
    );
    
    FrameGraphTypes::NodeId graphicsNodeId = frameGraph->addNode<EntityGraphicsNode>(
        entityBufferId,
        positionBufferId,
        swapchainImageId,
        pipeline.get(),
        swapchain.get(),
        resourceContext.get(),
        gpuEntityManager.get()
    );
    
    FrameGraphTypes::NodeId presentNodeId = frameGraph->addNode<SwapchainPresentNode>(
        swapchainImageId,
        swapchain.get()
    );
    
    // Set the correct image index for the current frame and world reference
    if (EntityGraphicsNode* graphicsNode = frameGraph->getNode<EntityGraphicsNode>(graphicsNodeId)) {
        graphicsNode->setImageIndex(imageIndex);
        graphicsNode->setWorld(world);
    }
    if (SwapchainPresentNode* presentNode = frameGraph->getNode<SwapchainPresentNode>(presentNodeId)) {
        presentNode->setImageIndex(imageIndex);
    }
    
    // 6. Compile and execute frame graph
    if (!frameGraph->compile()) {
        std::cerr << "Failed to compile frame graph" << std::endl;
        return;
    }
    
    // 6. Update frame data and reset command buffers
    totalTime += deltaTime;
    frameCounter++;
    
    const auto& computeCommandBuffers = sync->getComputeCommandBuffers();
    const auto& commandBuffers = sync->getCommandBuffers();
    context->getLoader().vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0);
    context->getLoader().vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    
    // Update frame data in nodes (AAA pattern: frame graph manages per-frame data)
    frameGraph->updateFrameData(totalTime, deltaTime, currentFrame);
    
    auto executionResult = frameGraph->execute(currentFrame);
    
    // 7. Submit compute work with fence if compute commands were recorded
    if (executionResult.computeCommandBufferUsed) {
        VkFence computeFence = frameFences.getComputeFence(currentFrame);
        VkResult resetResult = context->getLoader().vkResetFences(context->getDevice(), 1, &computeFence);
        if (resetResult != VK_SUCCESS) {
            std::cerr << "Failed to reset compute fence: " << resetResult << std::endl;
            return;
        }
        
        VkSubmitInfo computeSubmitInfo{};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
        
        VkResult computeSubmitResult = context->getLoader().vkQueueSubmit(context->getGraphicsQueue(), 1, &computeSubmitInfo, computeFence);
        if (computeSubmitResult != VK_SUCCESS) {
            std::cerr << "Failed to submit compute commands: " << computeSubmitResult << std::endl;
            return;
        }
        frameFences.setComputeInUse(currentFrame, true);
    }

    // 8. Submit graphics work with fence and semaphores if graphics commands were recorded
    // 9. Present only if graphics work was submitted
    if (executionResult.graphicsCommandBufferUsed) {
        VkFence graphicsFence = frameFences.getGraphicsFence(currentFrame);
        VkResult graphicsResetResult = context->getLoader().vkResetFences(context->getDevice(), 1, &graphicsFence);
        if (graphicsResetResult != VK_SUCCESS) {
            std::cerr << "Failed to reset graphics fence: " << graphicsResetResult << std::endl;
            return;
        }
        
        VkSubmitInfo graphicsSubmitInfo{};
        graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {sync->getImageAvailableSemaphores()[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        graphicsSubmitInfo.waitSemaphoreCount = 1;
        graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
        graphicsSubmitInfo.pWaitDstStageMask = waitStages;
        graphicsSubmitInfo.commandBufferCount = 1;
        graphicsSubmitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {sync->getRenderFinishedSemaphores()[currentFrame]};
        graphicsSubmitInfo.signalSemaphoreCount = 1;
        graphicsSubmitInfo.pSignalSemaphores = signalSemaphores;

        VkResult graphicsSubmitResult = context->getLoader().vkQueueSubmit(context->getGraphicsQueue(), 1, &graphicsSubmitInfo, graphicsFence);
        if (graphicsSubmitResult != VK_SUCCESS) {
            std::cerr << "Failed to submit graphics commands: " << graphicsSubmitResult << std::endl;
            return;
        }
        frameFences.setGraphicsInUse(currentFrame, true);
        
        // Present (only if graphics was submitted)
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = context->getLoader().vkQueuePresentKHR(context->getPresentQueue(), &presentInfo);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            std::cerr << "Failed to present swap chain image: " << result << std::endl;
        }
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

