#include "vulkan_renderer.h"
#include "vulkan/vulkan_function_loader.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_resources.h"
#include "vulkan/vulkan_sync.h"
#include "vulkan/resource_context.h"
#include "ecs/gpu_entity_manager.h"
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
    
    resources = std::make_unique<VulkanResources>();
    if (!resources->initialize(*context, sync.get(), resourceContext.get())) {
        std::cerr << "Failed to initialize Vulkan resources" << std::endl;
        return false;
    }
    
    if (!resources->createUniformBuffers()) {
        std::cerr << "Failed to create uniform buffers" << std::endl;
        return false;
    }
    
    if (!resources->createTriangleBuffers()) {
        std::cerr << "Failed to create triangle buffers" << std::endl;
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
    
    // Initialize GPU entity manager after sync and resource context are created
    gpuEntityManager = std::make_unique<GPUEntityManager>();
    if (!gpuEntityManager->initialize(*context, sync.get(), resourceContext.get(), pipeline->getComputeDescriptorSetLayout())) {
        std::cerr << "Failed to initialize GPU entity manager" << std::endl;
        return false;
    }
    
    // Update graphics descriptor sets with position buffer for compute-based rendering
    if (!resources->updateDescriptorSetsWithPositionBuffer(gpuEntityManager->getCurrentPositionBuffer())) {
        std::cerr << "Failed to update descriptor sets with position buffer" << std::endl;
        return false;
    }
    std::cout << "Graphics descriptor sets updated with position buffer" << std::endl;
    
    // Initialize movement command processor
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    std::cout << "Movement command processor initialized" << std::endl;
    
    
    
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
    
    movementCommandProcessor.reset();
    gpuEntityManager.reset();
    sync.reset();
    resources.reset();
    pipeline.reset();
    swapchain.reset();
    context.reset();
    
    initialized = false;
}

void VulkanRenderer::drawFrame() {
    // Get sync objects
    const auto& imageAvailableSemaphores = sync->getImageAvailableSemaphores();
    const auto& renderFinishedSemaphores = sync->getRenderFinishedSemaphores();
    const auto& commandBuffers = sync->getCommandBuffers();
    const auto& computeCommandBuffers = sync->getComputeCommandBuffers();

    // 1. Ensure GPU is done with THIS frame's resources
    // Use immediate check first, then longer timeout if needed for smooth 60fps
    
    if (frameFences.isComputeInUse(currentFrame)) {
        VkResult computeResult = waitForFenceRobust(frameFences.getComputeFence(currentFrame), "compute");
#ifdef _DEBUG
        assert(computeResult == VK_SUCCESS || computeResult == VK_ERROR_DEVICE_LOST);
#endif
        if (computeResult == VK_ERROR_DEVICE_LOST) {
            frameFences.setComputeInUse(currentFrame, false);
            return;
        }
        frameFences.setComputeInUse(currentFrame, false);
    }
    
    if (frameFences.isGraphicsInUse(currentFrame)) {
        VkResult graphicsResult = waitForFenceRobust(frameFences.getGraphicsFence(currentFrame), "graphics");
#ifdef _DEBUG
        assert(graphicsResult == VK_SUCCESS || graphicsResult == VK_ERROR_DEVICE_LOST);
#endif
        if (graphicsResult == VK_ERROR_DEVICE_LOST) {
            frameFences.setGraphicsInUse(currentFrame, false);
            return;
        }
        frameFences.setGraphicsInUse(currentFrame, false);
    }

    // 2. Process movement commands at very beginning of frame for complete buffer coverage
    if (movementCommandProcessor) {
        movementCommandProcessor->processCommands();
    }

    // 3. Acquire swapchain image
    uint32_t imageIndex;
    VkResult result = context->getLoader().vkAcquireNextImageKHR(context->getDevice(), swapchain->getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

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

    // 5. Record compute command buffer
    context->getLoader().vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0);
    
    VkCommandBufferBeginInfo computeBeginInfo{};
    computeBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    VkResult beginResult = context->getLoader().vkBeginCommandBuffer(computeCommandBuffers[currentFrame], &computeBeginInfo);
#ifdef _DEBUG
    assert(beginResult == VK_SUCCESS);
#else
    (void)beginResult;
#endif
    
    // Dispatch compute shader for movement calculation
    if (gpuEntityManager && gpuEntityManager->getEntityCount() > 0) {
        // Bind unified compute pipeline
        context->getLoader().vkCmdBindPipeline(computeCommandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getComputePipeline());
        
        // Bind compute descriptor set
        VkDescriptorSet computeDescriptorSet = gpuEntityManager->getCurrentComputeDescriptorSet();
        context->getLoader().vkCmdBindDescriptorSets(
            computeCommandBuffers[currentFrame], 
            VK_PIPELINE_BIND_POINT_COMPUTE, 
            pipeline->getComputePipelineLayout(), 
            0, 1, &computeDescriptorSet, 0, nullptr
        );
        
        // Update push constants for compute shader
        struct ComputePushConstants {
            float time;
            float deltaTime;
            uint32_t entityCount;
            uint32_t frame;
        } computePushConstants;
        
        computePushConstants.time = totalTime;
        computePushConstants.deltaTime = deltaTime;
        computePushConstants.entityCount = gpuEntityManager->getEntityCount();
        computePushConstants.frame = frameCounter;
        
        context->getLoader().vkCmdPushConstants(
            computeCommandBuffers[currentFrame],
            pipeline->getComputePipelineLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ComputePushConstants),
            &computePushConstants
        );
        
        // Dispatch compute shader (32 threads per workgroup)
        uint32_t numWorkgroups = (gpuEntityManager->getEntityCount() + 31) / 32;
        context->getLoader().vkCmdDispatch(computeCommandBuffers[currentFrame], numWorkgroups, 1, 1);
    }
    
    // Update total simulation time
    totalTime += deltaTime;
    
    // Memory barrier to ensure compute write completes before graphics read
    transitionBufferLayout(computeCommandBuffers[currentFrame]);
    
    // Update frame counter
    frameCounter++;
    
    VkResult endResult = context->getLoader().vkEndCommandBuffer(computeCommandBuffers[currentFrame]);
#ifdef _DEBUG
    assert(endResult == VK_SUCCESS);
#else
    (void)endResult;
#endif

    // 6. Record graphics command buffer
    context->getLoader().vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    // 7. Submit compute work with its own fence - NO BLOCKING
    VkFence computeFence = frameFences.getComputeFence(currentFrame);
    context->getLoader().vkResetFences(context->getDevice(), 1, &computeFence);
    
    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
    
    VkResult computeSubmitResult = context->getLoader().vkQueueSubmit(context->getGraphicsQueue(), 1, &computeSubmitInfo, computeFence);
#ifdef _DEBUG
    assert(computeSubmitResult == VK_SUCCESS);
#else
    (void)computeSubmitResult;
#endif
    frameFences.setComputeInUse(currentFrame, true);

    // 8. Submit graphics work with its own fence - NO BLOCKING
    VkFence graphicsFence = frameFences.getGraphicsFence(currentFrame);
    context->getLoader().vkResetFences(context->getDevice(), 1, &graphicsFence);
    
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

    VkResult graphicsSubmitResult = context->getLoader().vkQueueSubmit(context->getGraphicsQueue(), 1, &graphicsSubmitInfo, graphicsFence);
#ifdef _DEBUG
    assert(graphicsSubmitResult == VK_SUCCESS);
#else
    (void)graphicsSubmitResult;
#endif
    frameFences.setGraphicsInUse(currentFrame, true);

    // 9. Present immediately - no fence waiting
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
        std::cerr << "Failed to present swap chain image" << std::endl;
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
        gpuEntityManager->setComputeDescriptorSetLayout(pipeline->getComputeDescriptorSetLayout());
        if (!gpuEntityManager->recreateComputeDescriptorResources()) {
            std::cerr << "Failed to recreate GPU entity manager descriptor sets!" << std::endl;
            recreationInProgress = false;
            return false;
        }
        
        // Update graphics descriptor sets with position buffer after recreation
        if (!resources->updateDescriptorSetsWithPositionBuffer(gpuEntityManager->getCurrentPositionBuffer())) {
            std::cerr << "Failed to update graphics descriptor sets with position buffer!" << std::endl;
            recreationInProgress = false;
            return false;
        }
    }

    recreationInProgress = false;
    return true;
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Begin graphics command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    VkResult graphicsBeginResult = context->getLoader().vkBeginCommandBuffer(commandBuffer, &beginInfo);
#ifdef _DEBUG
    assert(graphicsBeginResult == VK_SUCCESS);
#else
    (void)graphicsBeginResult;
#endif

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

    context->getLoader().vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    context->getLoader().vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getGraphicsPipeline());
    
    const auto& descriptorSets = resources->getDescriptorSets();
    context->getLoader().vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // Push constants for vertex shader
    struct VertexPushConstants {
        float time;                 // Current simulation time
        float dt;                   // Time per frame  
        uint32_t count;             // Total number of entities
    } vertexPushConstants = { 
        totalTime,
        deltaTime,
        gpuEntityManager ? gpuEntityManager->getEntityCount() : 0
    };
    
    context->getLoader().vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(),
                      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertexPushConstants), &vertexPushConstants);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchain->getExtent().width;
    viewport.height = (float) swapchain->getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    context->getLoader().vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    context->getLoader().vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    // GPU-only rendering - all entities are triangles
    uint32_t gpuEntityCount = gpuEntityManager ? gpuEntityManager->getEntityCount() : 0;
    if (gpuEntityCount > 0) {
        VkBuffer vertexBuffers[] = {resources->getVertexBuffer(), gpuEntityManager->getCurrentEntityBuffer()};
        VkDeviceSize offsets[] = {0, 0};
        context->getLoader().vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
        context->getLoader().vkCmdBindIndexBuffer(commandBuffer, resources->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        context->getLoader().vkCmdDrawIndexed(commandBuffer, resources->getIndexCount(), gpuEntityCount, 0, 0, 0);
    }

    context->getLoader().vkCmdEndRenderPass(commandBuffer);

    VkResult graphicsEndResult = context->getLoader().vkEndCommandBuffer(commandBuffer);
#ifdef _DEBUG
    assert(graphicsEndResult == VK_SUCCESS);
#else
    (void)graphicsEndResult;
#endif
}


void VulkanRenderer::uploadPendingGPUEntities() {
    if (gpuEntityManager) {
        gpuEntityManager->uploadPendingEntities();
    }
}



void VulkanRenderer::transitionBufferLayout(VkCommandBuffer commandBuffer) {
    if (!gpuEntityManager || gpuEntityManager->getEntityCount() == 0) {
        return;
    }
    
    // Buffer memory barriers to ensure proper synchronization between compute and graphics stages
    std::array<VkBufferMemoryBarrier, 4> bufferBarriers{};
    
    // GPUEntity buffer barrier (compute read/write -> vertex shader instance data read)
    bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[0].buffer = gpuEntityManager->getCurrentEntityBuffer();
    bufferBarriers[0].offset = 0;
    bufferBarriers[0].size = VK_WHOLE_SIZE;
    
    // Position buffer barrier (compute write -> vertex shader storage buffer read)
    bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[1].buffer = gpuEntityManager->getCurrentPositionBuffer();
    bufferBarriers[1].offset = 0;
    bufferBarriers[1].size = VK_WHOLE_SIZE;
    
    // Current position buffer barrier (compute read/write -> potential next frame access)
    bufferBarriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[2].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[2].buffer = gpuEntityManager->getCurrentPositionStorageBuffer();
    bufferBarriers[2].offset = 0;
    bufferBarriers[2].size = VK_WHOLE_SIZE;
    
    // Target position buffer barrier (compute read/write -> potential next frame access)
    bufferBarriers[3].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarriers[3].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarriers[3].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[3].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarriers[3].buffer = gpuEntityManager->getTargetPositionStorageBuffer();
    bufferBarriers[3].offset = 0;
    bufferBarriers[3].size = VK_WHOLE_SIZE;
    
    context->getLoader().vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                        0, 
                        0, nullptr,                    // No general memory barriers
                        bufferBarriers.size(), bufferBarriers.data(),  // Buffer barriers
                        0, nullptr);                   // No image barriers
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

