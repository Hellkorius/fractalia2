#include "vulkan_renderer.h"
#include "vulkan/vulkan_function_loader.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_resources.h"
#include "vulkan/vulkan_sync.h"
#include "vulkan/compute_pipeline.h"
#include "ecs/gpu_entity_manager.h"
#include "ecs/systems/camera_system.hpp"
#include "ecs/camera_component.hpp"
#include "ecs/movement_command_system.hpp"
#include <iostream>
#include <array>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Instance data structure for GPU upload
struct InstanceData {
    glm::mat4 transform;
    glm::vec4 color;
};

VulkanRenderer::VulkanRenderer() {
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::initialize(SDL_Window* window) {
    this->window = window;
    
    // Initialize per-frame data
    frameData.resize(MAX_FRAMES_IN_FLIGHT);
    
    // Initialize centralized function loader first
    functionLoader = std::make_unique<VulkanFunctionLoader>();
    if (!functionLoader->initialize(window)) {
        std::cerr << "Failed to initialize Vulkan function loader" << std::endl;
        return false;
    }
    
    context = std::make_unique<VulkanContext>();
    if (!context->initialize(window, functionLoader.get())) {
        std::cerr << "Failed to initialize Vulkan context" << std::endl;
        return false;
    }
    
    swapchain = std::make_unique<VulkanSwapchain>();
    if (!swapchain->initialize(context.get(), window, functionLoader.get())) {
        std::cerr << "Failed to initialize Vulkan swapchain" << std::endl;
        return false;
    }
    
    pipeline = std::make_unique<VulkanPipeline>();
    if (!pipeline->initialize(context.get(), swapchain->getImageFormat(), functionLoader.get())) {
        std::cerr << "Failed to initialize Vulkan pipeline" << std::endl;
        return false;
    }
    
    if (!swapchain->createFramebuffers(pipeline->getRenderPass())) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return false;
    }
    
    sync = std::make_unique<VulkanSync>();
    if (!sync->initialize(context.get())) {
        std::cerr << "Failed to initialize Vulkan sync" << std::endl;
        return false;
    }
    
    resources = std::make_unique<VulkanResources>();
    if (!resources->initialize(context.get(), sync.get())) {
        std::cerr << "Failed to initialize Vulkan resources" << std::endl;
        return false;
    }
    
    if (!resources->createUniformBuffers()) {
        std::cerr << "Failed to create uniform buffers" << std::endl;
        return false;
    }
    
    if (!resources->createVertexBuffer()) {
        std::cerr << "Failed to create vertex buffer" << std::endl;
        return false;
    }
    
    if (!resources->createIndexBuffer()) {
        std::cerr << "Failed to create index buffer" << std::endl;
        return false;
    }
    
    if (!resources->createInstanceBuffers()) {
        std::cerr << "Failed to create instance buffers" << std::endl;
        return false;
    }
    
    if (!resources->createKeyframeBuffers()) {
        std::cerr << "Failed to create keyframe buffers" << std::endl;
        return false;
    }
    
    if (!resources->createKeyframeDescriptorPool()) {
        std::cerr << "Failed to create keyframe descriptor pool" << std::endl;
        return false;
    }
    
    if (!resources->createKeyframeDescriptorSetLayout()) {
        std::cerr << "Failed to create keyframe descriptor set layout" << std::endl;
        return false;
    }
    
    if (!resources->createKeyframeDescriptorSets()) {
        std::cerr << "Failed to create keyframe descriptor sets" << std::endl;
        return false;
    }
    
    if (!resources->createDescriptorPool(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }
    
    if (!resources->createDescriptorSets(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor sets" << std::endl;
        return false;
    }
    
    // Initialize GPU entity manager after sync is created
    gpuEntityManager = std::make_unique<GPUEntityManager>();
    if (!gpuEntityManager->initialize(context.get(), sync.get(), functionLoader.get())) {
        std::cerr << "Failed to initialize GPU entity manager" << std::endl;
        return false;
    }
    
    // Update keyframe descriptor set with entity buffer
    resources->updateKeyframeDescriptorSet(gpuEntityManager->getCurrentEntityBuffer());
    
    // Initialize movement command processor
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    std::cout << "Movement command processor initialized" << std::endl;
    
    // Initialize compute pipeline
    computePipeline = std::make_unique<ComputePipeline>();
    if (!computePipeline->initialize(context.get(), functionLoader.get())) {
        std::cerr << "Failed to initialize compute pipeline" << std::endl;
        return false;
    }
    
    
    if (!computePipeline->createKeyframePipeline(resources->getKeyframeDescriptorSetLayout())) {
        std::cerr << "Failed to create keyframe compute pipeline" << std::endl;
        return false;
    }
    
    loadDrawingFunctions();
    
    // Initialize per-frame fences
    if (!initializeFrameFences()) {
        std::cerr << "Failed to initialize frame fences" << std::endl;
        return false;
    }
    
    // Initialize buffer capacity limits after all resources are created
    maxCpuInstances = resources->getMaxInstancesPerBuffer();
    std::cout << "CPU instance buffer capacity: " << maxCpuInstances << " entities" << std::endl;
    
    initialized = true;
    return true;
}

void VulkanRenderer::cleanup() {
    if (functionLoader && context && context->getDevice() != VK_NULL_HANDLE) {
        functionLoader->vkDeviceWaitIdle(context->getDevice());
        
        // Clean up per-frame fences with proper state validation
        for (auto& frame : frameData) {
            if (frame.fencesInitialized) {
                if (frame.computeDone != VK_NULL_HANDLE) {
                    functionLoader->vkDestroyFence(context->getDevice(), frame.computeDone, nullptr);
                    frame.computeDone = VK_NULL_HANDLE;
                }
                if (frame.graphicsDone != VK_NULL_HANDLE) {
                    functionLoader->vkDestroyFence(context->getDevice(), frame.graphicsDone, nullptr);
                    frame.graphicsDone = VK_NULL_HANDLE;
                }
                frame.fencesInitialized = false;
            }
        }
    }
    
    movementCommandProcessor.reset();
    gpuEntityManager.reset();
    computePipeline.reset();
    sync.reset();
    resources.reset();
    pipeline.reset();
    swapchain.reset();
    context.reset();
    functionLoader.reset();
    
    initialized = false;
}

void VulkanRenderer::drawFrame() {
    // Get current frame data and sync objects
    FrameData& frame = frameData[currentFrame];
    const auto& imageAvailableSemaphores = sync->getImageAvailableSemaphores();
    const auto& renderFinishedSemaphores = sync->getRenderFinishedSemaphores();
    const auto& commandBuffers = sync->getCommandBuffers();
    const auto& computeCommandBuffers = sync->getComputeCommandBuffers();

    // 1. Ensure GPU is done with THIS frame's resources
    // Use immediate check first, then longer timeout if needed for smooth 60fps
    
    if (frame.computeInUse) {
        VkResult computeResult = waitForFenceRobust(frame.computeDone, "compute");
        if (computeResult == VK_ERROR_DEVICE_LOST) {
            frame.computeInUse = false;
            return; // Critical error, skip frame
        }
        if (computeResult != VK_SUCCESS) {
            std::cerr << "Error: Compute fence wait failed: " << computeResult << std::endl;
            return;
        }
        
        frame.computeInUse = false;
    }
    
    if (frame.graphicsInUse) {
        VkResult graphicsResult = waitForFenceRobust(frame.graphicsDone, "graphics");
        if (graphicsResult == VK_ERROR_DEVICE_LOST) {
            frame.graphicsInUse = false;
            return; // Critical error, skip frame
        }
        if (graphicsResult != VK_SUCCESS) {
            std::cerr << "Error: Graphics fence wait failed: " << graphicsResult << std::endl;
            return;
        }
        
        frame.graphicsInUse = false;
    }

    // 2. Process movement commands at very beginning of frame for complete buffer coverage
    if (movementCommandProcessor) {
        movementCommandProcessor->processCommands();
    }

    // 3. Acquire swapchain image
    uint32_t imageIndex;
    VkResult result = functionLoader->vkAcquireNextImageKHR(context->getDevice(), swapchain->getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Failed to acquire swap chain image" << std::endl;
        return;
    }

    // 4. Upload pending data and update buffers
    uploadPendingGPUEntities();
    updateUniformBuffer(currentFrame);
    updateInstanceBuffer(currentFrame);

    // 5. Record compute command buffer
    functionLoader->vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0);
    
    VkCommandBufferBeginInfo computeBeginInfo{};
    computeBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (functionLoader->vkBeginCommandBuffer(computeCommandBuffers[currentFrame], &computeBeginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin recording compute command buffer!" << std::endl;
        return;
    }
    
    // Using keyframe-based rendering
    
    // Update total simulation time
    totalTime += deltaTime;
    
    // Staggered updates: only update entities whose ID matches current frame modulo
    // Each entity gets updated every KEYFRAME_LOOKAHEAD_FRAMES frames
    uint32_t entityBatch = frameCounter % KEYFRAME_LOOKAHEAD_FRAMES;
    float futureTime = totalTime + KEYFRAME_LOOKAHEAD_FRAMES * deltaTime;
    
    dispatchKeyframeCompute(computeCommandBuffers[currentFrame], futureTime, deltaTime, entityBatch);
    
    transitionBufferLayout(computeCommandBuffers[currentFrame]);
    
    // Update frame counter
    frameCounter++;
    
    if (functionLoader->vkEndCommandBuffer(computeCommandBuffers[currentFrame]) != VK_SUCCESS) {
        std::cerr << "Failed to record compute command buffer" << std::endl;
        return;
    }

    // 6. Record graphics command buffer
    functionLoader->vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    // 7. Submit compute work with its own fence - NO BLOCKING
    functionLoader->vkResetFences(context->getDevice(), 1, &frame.computeDone);
    
    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
    
    if (functionLoader->vkQueueSubmit(context->getGraphicsQueue(), 1, &computeSubmitInfo, frame.computeDone) != VK_SUCCESS) {
        std::cerr << "Failed to submit compute command buffer" << std::endl;
        return;
    }
    frame.computeInUse = true;

    // 8. Submit graphics work with its own fence - NO BLOCKING
    functionLoader->vkResetFences(context->getDevice(), 1, &frame.graphicsDone);
    
    VkSubmitInfo graphicsSubmitInfo{};
    graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    graphicsSubmitInfo.waitSemaphoreCount = 1;
    graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
    graphicsSubmitInfo.pWaitDstStageMask = waitStages;
    graphicsSubmitInfo.commandBufferCount = 1;
    graphicsSubmitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    graphicsSubmitInfo.signalSemaphoreCount = 1;
    graphicsSubmitInfo.pSignalSemaphores = signalSemaphores;

    if (functionLoader->vkQueueSubmit(context->getGraphicsQueue(), 1, &graphicsSubmitInfo, frame.graphicsDone) != VK_SUCCESS) {
        std::cerr << "Failed to submit graphics command buffer" << std::endl;
        return;
    }
    frame.graphicsInUse = true;

    // 9. Present immediately - no fence waiting
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = functionLoader->vkQueuePresentKHR(context->getPresentQueue(), &presentInfo);
    
    // Low-latency optimization: release swapchain images immediately after present
    // TEMPORARILY DISABLED - may be causing synchronization issues
    // if (functionLoader->vkReleaseSwapchainImagesEXT && result == VK_SUCCESS) {
    //     VkReleaseSwapchainImagesInfoEXT releaseInfo{};
    //     releaseInfo.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT;
    //     releaseInfo.swapchain = swapchain->getSwapchain();
    //     releaseInfo.imageIndexCount = 1;
    //     releaseInfo.pImageIndices = &imageIndex;
    //     
    //     functionLoader->vkReleaseSwapchainImagesEXT(context->getDevice(), &releaseInfo);
    // }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        std::cerr << "Failed to present swap chain image" << std::endl;
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanRenderer::recreateSwapChain() {
    if (!swapchain->recreate(pipeline->getRenderPass())) {
        return false;
    }
    
    if (!pipeline->recreate(swapchain->getImageFormat())) {
        return false;
    }
    
    if (!swapchain->createFramebuffers(pipeline->getRenderPass())) {
        return false;
    }

    return true;
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Begin graphics command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (functionLoader->vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin recording graphics command buffer!" << std::endl;
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = pipeline->getRenderPass();
    renderPassInfo.framebuffer = swapchain->getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->getExtent();

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.2f, 1.0f}};
    clearValues[1].color = {{0.1f, 0.1f, 0.2f, 1.0f}};
    clearValues[2].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    functionLoader->vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    functionLoader->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getGraphicsPipeline());
    
    const auto& descriptorSets = resources->getDescriptorSets();
    functionLoader->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // Push constants for vertex shader keyframe interpolation
    static float lastPredictionTime = totalTime + KEYFRAME_LOOKAHEAD_FRAMES * deltaTime;
    
    struct VertexPushConstants {
        float totalTime;            // Current simulation time
        float deltaTime;            // Time per frame
        float predictionTime;       // Time the keyframes were predicted for
        uint32_t entityCount;       // Total number of entities
    } vertexPushConstants = { 
        totalTime,
        deltaTime,
        lastPredictionTime,
        gpuEntityManager ? gpuEntityManager->getEntityCount() : 0
    };
    
    functionLoader->vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(),
                      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertexPushConstants), &vertexPushConstants);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchain->getExtent().width;
    viewport.height = (float) swapchain->getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    functionLoader->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    functionLoader->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    // Render all entities by type
    uint32_t instanceOffset = 0;
    
    // Count entities by type with robust bounds checking
    uint32_t triangleCount = 0;
    uint32_t squareCount = 0;
    const uint32_t maxInstances = resources->getMaxInstancesPerBuffer();
    const uint32_t entityCount = static_cast<uint32_t>(renderEntities.size());
    
    // Validate that renderEntities hasn't exceeded buffer capacity (should never happen after our fixes)
    if (entityCount > maxInstances) {
        std::cerr << "FATAL: renderEntities size (" << entityCount 
                  << ") exceeds buffer capacity (" << maxInstances 
                  << ") in recordCommandBuffer!" << std::endl;
        std::cerr << "This indicates a critical bug in entity capacity validation!" << std::endl;
    }
    
    // Count entities with safety bounds - process only what actually fits
    uint32_t totalProcessed = 0;
    for (const auto& [pos, shapeType, color] : renderEntities) {
        if (totalProcessed >= maxInstances) {
            std::cerr << "CRITICAL: Stopping entity counting at " << totalProcessed 
                      << " to prevent buffer overflow" << std::endl;
            break;
        }
        
        if (shapeType == ShapeType::Triangle) {
            triangleCount++;
        } else if (shapeType == ShapeType::Square) {
            squareCount++;
        }
        totalProcessed++;
    }
    
    // Final safety clamps (should be redundant but provides extra protection)
    triangleCount = std::min(triangleCount, maxInstances);
    squareCount = std::min(squareCount, maxInstances - triangleCount);
    
    // Log entity counts for debugging
    static uint32_t lastTriangleCount = 0, lastSquareCount = 0;
    if (triangleCount != lastTriangleCount || squareCount != lastSquareCount) {
        std::cout << "Render counts - Triangles: " << triangleCount 
                  << ", Squares: " << squareCount << "/" << maxInstances << std::endl;
        lastTriangleCount = triangleCount;
        lastSquareCount = squareCount;
    }
    
    
    // Render triangles using GPU entity buffer
    uint32_t gpuEntityCount = gpuEntityManager ? gpuEntityManager->getEntityCount() : 0;
    if (gpuEntityCount > 0) {
        VkBuffer triangleVertexBuffers[] = {resources->getTriangleVertexBuffer(), gpuEntityManager->getCurrentEntityBuffer()};
        VkDeviceSize offsets[] = {0, 0}; // GPU entities start at beginning of buffer
        functionLoader->vkCmdBindVertexBuffers(commandBuffer, 0, 2, triangleVertexBuffers, offsets);
        functionLoader->vkCmdBindIndexBuffer(commandBuffer, resources->getTriangleIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        functionLoader->vkCmdDrawIndexed(commandBuffer, resources->getTriangleIndexCount(), gpuEntityCount, 0, 0, 0);
    } else if (triangleCount > 0) {
        // Fallback to old CPU instance buffer for compatibility
        VkBuffer triangleVertexBuffers[] = {resources->getTriangleVertexBuffer(), resources->getInstanceBuffers()[currentFrame]};
        VkDeviceSize offsets[] = {0, instanceOffset * sizeof(InstanceData)};
        functionLoader->vkCmdBindVertexBuffers(commandBuffer, 0, 2, triangleVertexBuffers, offsets);
        functionLoader->vkCmdBindIndexBuffer(commandBuffer, resources->getTriangleIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        functionLoader->vkCmdDrawIndexed(commandBuffer, resources->getTriangleIndexCount(), triangleCount, 0, 0, 0);
        instanceOffset += triangleCount;
    }
    
	// Skip squares when using GPU entities - all entities are rendered as triangles for simplicity
	// GPU compute shader doesn't differentiate by shape type in this implementation
	if (gpuEntityCount == 0 && squareCount > 0) {
		// Fallback CPU rendering for squares
		VkBuffer squareVertexBuffers[] = {resources->getSquareVertexBuffer(), resources->getInstanceBuffers()[currentFrame]};
		VkDeviceSize offsets[] = {0, instanceOffset * sizeof(InstanceData)};
		functionLoader->vkCmdBindVertexBuffers(commandBuffer, 0, 2, squareVertexBuffers, offsets);
		functionLoader->vkCmdBindIndexBuffer(commandBuffer, resources->getSquareIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
		functionLoader->vkCmdDrawIndexed(commandBuffer, resources->getSquareIndexCount(), squareCount, 0, 0, 0);
		instanceOffset += squareCount;
	}

    // Emergency test: Draw a simple triangle without instancing
    if (triangleCount == 0 && squareCount == 0) {
        VkBuffer testVertexBuffer = resources->getTriangleVertexBuffer();
        VkDeviceSize testOffset = 0;
        functionLoader->vkCmdBindVertexBuffers(commandBuffer, 0, 1, &testVertexBuffer, &testOffset);
        functionLoader->vkCmdBindIndexBuffer(commandBuffer, resources->getTriangleIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        functionLoader->vkCmdDrawIndexed(commandBuffer, resources->getTriangleIndexCount(), 1, 0, 0, 0);
    }

    functionLoader->vkCmdEndRenderPass(commandBuffer);

    if (functionLoader->vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to record graphics command buffer" << std::endl;
    }
}

void VulkanRenderer::loadDrawingFunctions() {
    // Note: Function loading now handled by centralized VulkanFunctionLoader
    // All Vulkan calls in this class now use functionLoader->functionName()
}

void VulkanRenderer::uploadPendingGPUEntities() {
    if (gpuEntityManager) {
        bool hadEntities = gpuEntityManager->getEntityCount() > 0;
        gpuEntityManager->uploadPendingEntities();
        
        // Initialize keyframes when entities are first uploaded
        if (!hadEntities && gpuEntityManager->getEntityCount() > 0) {
            initializeAllKeyframes();
        }
    }
}


void VulkanRenderer::dispatchKeyframeCompute(VkCommandBuffer commandBuffer, float futureTime, float deltaTime, uint32_t entityBatch) {
    if (!gpuEntityManager || !computePipeline || gpuEntityManager->getEntityCount() == 0) {
        return;
    }
    
    // Bind keyframe compute pipeline
    functionLoader->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->getKeyframePipeline());
    
    // Bind keyframe descriptor set
    VkDescriptorSet keyframeDescriptorSet = resources->getKeyframeDescriptorSet();
    functionLoader->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, 
                           computePipeline->getKeyframePipelineLayout(), 0, 1, &keyframeDescriptorSet, 0, nullptr);
    
    // Push constants for keyframe generation
    struct KeyframePushConstants {
        uint32_t entityBatch;       // Which entity batch to update (0-19)
        float deltaTime;            // Time per frame
        uint32_t entityCount;       // Total number of entities
        float futureTime;           // Future prediction time
    } pushConstants = { 
        entityBatch, 
        deltaTime,
        gpuEntityManager->getEntityCount(),
        futureTime 
    };
    
    functionLoader->vkCmdPushConstants(commandBuffer, computePipeline->getKeyframePipelineLayout(),
                      VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(KeyframePushConstants), &pushConstants);
    
    // Dispatch compute shader (64 threads per workgroup)
    uint32_t workgroupCount = (gpuEntityManager->getEntityCount() + 63) / 64;
    functionLoader->vkCmdDispatch(commandBuffer, workgroupCount, 1, 1);
}

void VulkanRenderer::initializeAllKeyframes() {
    if (!gpuEntityManager || !computePipeline || gpuEntityManager->getEntityCount() == 0) {
        return;
    }
    
    // Create a temporary command buffer for keyframe initialization
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = sync->getCommandPool();
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer initCommandBuffer;
    if (functionLoader->vkAllocateCommandBuffers(context->getDevice(), &allocInfo, &initCommandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate keyframe initialization command buffer" << std::endl;
        return;
    }
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (functionLoader->vkBeginCommandBuffer(initCommandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin keyframe initialization command buffer" << std::endl;
        return;
    }
    
    // Compute all keyframe slots sequentially at startup
    float baseTime = 0.0f;
    float deltaTime = 1.0f / 60.0f; // Assume 60 FPS
    
    for (uint32_t slot = 0; slot < KEYFRAME_LOOKAHEAD_FRAMES; slot++) {
        dispatchKeyframeCompute(initCommandBuffer, baseTime, deltaTime, slot);
        
        // Add memory barrier between dispatches
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        functionLoader->vkCmdPipelineBarrier(initCommandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
    
    if (functionLoader->vkEndCommandBuffer(initCommandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to end keyframe initialization command buffer" << std::endl;
        return;
    }
    
    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &initCommandBuffer;
    
    if (functionLoader->vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        std::cerr << "Failed to submit keyframe initialization" << std::endl;
        return;
    }
    
    functionLoader->vkQueueWaitIdle(context->getGraphicsQueue());
    
    // Clean up
    functionLoader->vkFreeCommandBuffers(context->getDevice(), sync->getCommandPool(), 1, &initCommandBuffer);
}

void VulkanRenderer::transitionBufferLayout(VkCommandBuffer commandBuffer) {
    if (!gpuEntityManager || gpuEntityManager->getEntityCount() == 0) {
        return;
    }
    
    // Memory barrier to ensure compute write is finished before graphics read
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    functionLoader->vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VulkanRenderer::updateUniformBuffer(uint32_t currentImage) {
    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
    } ubo{};

    // Get camera matrices from ECS if available
    if (world != nullptr) {
        auto cameraMatrices = CameraManager::getCameraMatrices(*world);
        if (cameraMatrices.valid) {
            ubo.view = cameraMatrices.view;
            ubo.proj = cameraMatrices.projection;
        } else {
            // Fallback to default matrices if no camera found
            ubo.view = glm::mat4(1.0f);
            ubo.proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -5.0f, 5.0f);
            ubo.proj[1][1] *= -1; // Flip Y for Vulkan
        }
    } else {
        // Original fallback matrices when no world is set
        ubo.view = glm::mat4(1.0f);
        ubo.proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -5.0f, 5.0f);
        ubo.proj[1][1] *= -1; // Flip Y for Vulkan
    }
    
    // Update the uniform buffer for the current frame
    void* data = resources->getUniformBuffersMapped()[currentImage];
    memcpy(data, &ubo, sizeof(ubo));
}

void VulkanRenderer::updateInstanceBuffer(uint32_t currentFrame) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    void* data = resources->getInstanceBuffersMapped()[currentFrame];
    InstanceData* instances = static_cast<InstanceData*>(data);
    
    uint32_t instanceIndex = 0;
    const uint32_t maxInstances = resources->getMaxInstancesPerBuffer();
    
    // Early bounds validation - this should never fail if updateEntities() is working correctly
    uint32_t totalEntities = static_cast<uint32_t>(renderEntities.size());
    if (totalEntities > maxInstances) {
        std::cerr << "FATAL: Instance buffer overflow detected in updateInstanceBuffer!" << std::endl;
        std::cerr << "  This indicates a bug in entity capacity validation." << std::endl;
        std::cerr << "  Entities: " << totalEntities << ", Buffer capacity: " << maxInstances << std::endl;
        // Emergency truncation to prevent memory corruption
        std::cerr << "  Emergency truncation activated to prevent crashes." << std::endl;
        // We'll still process entities but with strict bounds checking below
    }
    
    // Process triangles first (to match render order) - with strict bounds enforcement
    for (const auto& [pos, shapeType, color] : renderEntities) {
        // Critical bounds check - this is our last line of defense against buffer overflow
        if (instanceIndex >= maxInstances) {
            std::cerr << "CRITICAL: Hard limit reached at instance " << instanceIndex 
                      << "/" << maxInstances << ". Stopping processing to prevent memory corruption." << std::endl;
            break;
        }
        
        if (shapeType == ShapeType::Triangle) {
            // Validate position values to prevent giant rectangles
            glm::vec3 safePos = pos;
            const float MAX_POS = 1000.0f; // Reasonable world limit
            safePos.x = glm::clamp(safePos.x, -MAX_POS, MAX_POS);
            safePos.y = glm::clamp(safePos.y, -MAX_POS, MAX_POS);
            safePos.z = glm::clamp(safePos.z, -MAX_POS, MAX_POS);
            
            glm::mat4 model = glm::translate(glm::mat4(1.0f), safePos);
            float rotationAngle = time * glm::radians(45.0f);
            // Validate rotation angle to prevent NaN/infinite values
            if (std::isfinite(rotationAngle)) {
                model = glm::rotate(model, rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f));
            }
            
            // Validate color values
            glm::vec4 safeColor = color;
            safeColor.r = glm::clamp(safeColor.r, 0.0f, 1.0f);
            safeColor.g = glm::clamp(safeColor.g, 0.0f, 1.0f);
            safeColor.b = glm::clamp(safeColor.b, 0.0f, 1.0f);
            safeColor.a = glm::clamp(safeColor.a, 0.0f, 1.0f);
            
            // Final safety check before writing to buffer
            if (instanceIndex < maxInstances) {
                instances[instanceIndex].transform = model;
                instances[instanceIndex].color = safeColor;
                instanceIndex++;
            } else {
                std::cerr << "EMERGENCY: Buffer bounds exceeded at triangle write. Skipping entity." << std::endl;
                break;
            }
        }
    }
    
    // Process squares second (to match render order) - with strict bounds enforcement
    for (const auto& [pos, shapeType, color] : renderEntities) {
        // Critical bounds check - this is our last line of defense against buffer overflow
        if (instanceIndex >= maxInstances) {
            std::cerr << "CRITICAL: Hard limit reached at instance " << instanceIndex 
                      << "/" << maxInstances << ". Stopping square processing to prevent memory corruption." << std::endl;
            break;
        }
        
        if (shapeType == ShapeType::Square) {
            // Validate position values to prevent giant rectangles
            glm::vec3 safePos = pos;
            const float MAX_POS = 1000.0f; // Reasonable world limit
            safePos.x = glm::clamp(safePos.x, -MAX_POS, MAX_POS);
            safePos.y = glm::clamp(safePos.y, -MAX_POS, MAX_POS);
            safePos.z = glm::clamp(safePos.z, -MAX_POS, MAX_POS);
            
            glm::mat4 model = glm::translate(glm::mat4(1.0f), safePos);
            float rotationAngle = time * glm::radians(-30.0f);
            // Validate rotation angle to prevent NaN/infinite values
            if (std::isfinite(rotationAngle)) {
                model = glm::rotate(model, rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f));
            }
            
            // Validate color values
            glm::vec4 safeColor = color;
            safeColor.r = glm::clamp(safeColor.r, 0.0f, 1.0f);
            safeColor.g = glm::clamp(safeColor.g, 0.0f, 1.0f);
            safeColor.b = glm::clamp(safeColor.b, 0.0f, 1.0f);
            safeColor.a = glm::clamp(safeColor.a, 0.0f, 1.0f);
            
            // Final safety check before writing to buffer
            if (instanceIndex < maxInstances) {
                instances[instanceIndex].transform = model;
                instances[instanceIndex].color = safeColor;
                instanceIndex++;
            } else {
                std::cerr << "EMERGENCY: Buffer bounds exceeded at square write. Skipping entity." << std::endl;
                break;
            }
        }
    }
    
    // Log final instance count for debugging
    static uint32_t lastInstanceCount = 0;
    if (instanceIndex != lastInstanceCount) {
        std::cout << "Instance buffer updated: " << instanceIndex << "/" << maxInstances << " entities processed" << std::endl;
        lastInstanceCount = instanceIndex;
    }
}

void VulkanRenderer::setEntityPosition(float x, float y, float z) {
    entityPosition = glm::vec3(x, y, z);
}

void VulkanRenderer::updateEntities(const std::vector<std::tuple<glm::vec3, ShapeType, glm::vec4>>& entities) {
    // Early validation: Check capacity before accepting entities
    uint32_t entityCount = static_cast<uint32_t>(entities.size());
    
    if (!validateEntityCapacity(entityCount, "updateEntities")) {
        // Truncate entities to fit within buffer capacity
        if (entityCount > maxCpuInstances) {
            renderEntities.clear();
            renderEntities.reserve(maxCpuInstances);
            
            // Copy only what fits, maintaining original order
            auto endIt = entities.begin() + maxCpuInstances;
            renderEntities.assign(entities.begin(), endIt);
            
            std::cerr << "ENTITY OVERFLOW: Truncated " << entityCount << " entities to " 
                      << maxCpuInstances << " to prevent buffer overflow" << std::endl;
        } else {
            renderEntities = entities;
        }
    } else {
        renderEntities = entities;
    }
}

bool VulkanRenderer::validateEntityCapacity(uint32_t entityCount, const char* source) const {
    if (entityCount > maxCpuInstances) {
        if (!hasBufferCapacityWarningShown) {
            std::cerr << "CRITICAL: Buffer capacity exceeded in " << source << "!" << std::endl;
            std::cerr << "  Requested: " << entityCount << " entities" << std::endl;
            std::cerr << "  Available: " << maxCpuInstances << " entities" << std::endl;
            std::cerr << "  This will cause memory corruption if not handled!" << std::endl;
            const_cast<VulkanRenderer*>(this)->hasBufferCapacityWarningShown = true;
        }
        return false;
    }
    return true;
}

bool VulkanRenderer::testBufferOverflowProtection() const {
    std::cout << "\n=== TESTING BUFFER OVERFLOW PROTECTION ===" << std::endl;
    std::cout << "Max CPU instances: " << maxCpuInstances << std::endl;
    
    if (!initialized) {
        std::cerr << "Cannot test - renderer not initialized!" << std::endl;
        return false;
    }
    
    bool allTestsPassed = true;
    
    // Test 1: Capacity validation function
    std::cout << "Test 1: Capacity validation function..." << std::endl;
    bool test1a = validateEntityCapacity(maxCpuInstances - 1, "test1a"); // Should pass
    bool test1b = validateEntityCapacity(maxCpuInstances, "test1b");     // Should pass (at limit)
    bool test1c = validateEntityCapacity(maxCpuInstances + 1, "test1c"); // Should fail
    
    if (test1a && test1b && !test1c) {
        std::cout << "  ✓ Capacity validation working correctly" << std::endl;
    } else {
        std::cerr << "  ✗ Capacity validation failed!" << std::endl;
        allTestsPassed = false;
    }
    
    // Test 2: Entity vector size validation
    std::cout << "Test 2: Current entity vector state..." << std::endl;
    uint32_t currentEntityCount = static_cast<uint32_t>(renderEntities.size());
    if (currentEntityCount <= maxCpuInstances) {
        std::cout << "  ✓ Current entities (" << currentEntityCount << ") within limits" << std::endl;
    } else {
        std::cerr << "  ✗ Current entities (" << currentEntityCount << ") exceed limits!" << std::endl;
        allTestsPassed = false;
    }
    
    // Test 3: GPU buffer limits
    std::cout << "Test 3: GPU entity buffer state..." << std::endl;
    if (gpuEntityManager) {
        uint32_t gpuEntityCount = gpuEntityManager->getEntityCount();
        uint32_t maxGpuEntities = gpuEntityManager->getMaxEntities();
        std::cout << "  GPU entities: " << gpuEntityCount << "/" << maxGpuEntities << std::endl;
        
        if (gpuEntityCount <= maxGpuEntities) {
            std::cout << "  ✓ GPU entities within limits" << std::endl;
        } else {
            std::cerr << "  ✗ GPU entities exceed buffer capacity!" << std::endl;
            allTestsPassed = false;
        }
    } else {
        std::cout << "  - GPU entity manager not available (using CPU path)" << std::endl;
    }
    
    // Test 4: Resource buffer consistency
    std::cout << "Test 4: Buffer consistency check..." << std::endl;
    if (resources) {
        uint32_t resourceMaxInstances = resources->getMaxInstancesPerBuffer();
        if (resourceMaxInstances == maxCpuInstances) {
            std::cout << "  ✓ Buffer size consistency verified" << std::endl;
        } else {
            std::cerr << "  ✗ Buffer size mismatch: cached=" << maxCpuInstances 
                      << ", actual=" << resourceMaxInstances << std::endl;
            allTestsPassed = false;
        }
    } else {
        std::cerr << "  ✗ Resources not available for testing!" << std::endl;
        allTestsPassed = false;
    }
    
    std::cout << "\n=== TEST RESULTS ===" << std::endl;
    if (allTestsPassed) {
        std::cout << "✅ ALL TESTS PASSED - Buffer overflow protection is working correctly!" << std::endl;
        std::cout << "Memory safety fixes have been successfully validated." << std::endl;
    } else {
        std::cerr << "❌ SOME TESTS FAILED - Buffer overflow protection needs attention!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
    
    return allTestsPassed;
}

void VulkanRenderer::updateAspectRatio(int windowWidth, int windowHeight) {
    if (world != nullptr) {
        CameraManager::updateAspectRatio(*world, windowWidth, windowHeight);
    }
}

bool VulkanRenderer::initializeFrameFences() {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled
    
    for (auto& frame : frameData) {
        if (functionLoader->vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &frame.computeDone) != VK_SUCCESS) {
            std::cerr << "Failed to create compute fence" << std::endl;
            // Clean up any previously created fences before returning
            cleanupPartialFrameFences();
            return false;
        }
        
        if (functionLoader->vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &frame.graphicsDone) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics fence" << std::endl;
            // Clean up compute fence just created and any previously created fences
            functionLoader->vkDestroyFence(context->getDevice(), frame.computeDone, nullptr);
            frame.computeDone = VK_NULL_HANDLE;
            cleanupPartialFrameFences();
            return false;
        }
        
        // Mark fences as successfully initialized for this frame
        frame.fencesInitialized = true;
    }
    
    return true;
}

void VulkanRenderer::cleanupPartialFrameFences() {
    for (auto& frame : frameData) {
        if (frame.fencesInitialized) {
            if (frame.computeDone != VK_NULL_HANDLE) {
                functionLoader->vkDestroyFence(context->getDevice(), frame.computeDone, nullptr);
                frame.computeDone = VK_NULL_HANDLE;
            }
            if (frame.graphicsDone != VK_NULL_HANDLE) {
                functionLoader->vkDestroyFence(context->getDevice(), frame.graphicsDone, nullptr);
                frame.graphicsDone = VK_NULL_HANDLE;
            }
            frame.fencesInitialized = false;
        }
    }
}

VkResult VulkanRenderer::waitForFenceRobust(VkFence fence, const char* fenceName) {
    // First try immediate check
    VkResult result = functionLoader->vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, FENCE_TIMEOUT_IMMEDIATE);
    if (result == VK_TIMEOUT) {
        // If not ready immediately, wait up to one frame time
        result = functionLoader->vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, FENCE_TIMEOUT_FRAME);
        if (result == VK_TIMEOUT) {
            // Proper timeout handling: reset fence and maintain synchronization
            std::cerr << "Warning: " << fenceName << " fence timeout, forcing synchronization" << std::endl;
            
            // Force wait with longer timeout to prevent desync
            result = functionLoader->vkWaitForFences(context->getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) {
                std::cerr << "Critical: Failed to synchronize " << fenceName << " pipeline: " << result << std::endl;
                // Reset state to prevent further issues
                functionLoader->vkResetFences(context->getDevice(), 1, &fence);
                return VK_ERROR_DEVICE_LOST; // Signal critical error
            }
        }
    }
    
    return result;
}

