#include "vulkan_renderer.h"
#include "vulkan/vulkan_function_loader.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_resources.h"
#include "vulkan/vulkan_sync.h"
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
    if (!resources->initialize(context.get(), sync.get(), functionLoader.get())) {
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
    
    if (!resources->createInstanceBuffer()) {
        std::cerr << "Failed to create instance buffers" << std::endl;
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
    
    
    // Initialize movement command processor
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    std::cout << "Movement command processor initialized" << std::endl;
    
    
    loadDrawingFunctions();
    
    // Initialize per-frame fences
    if (!initializeFrameFences()) {
        std::cerr << "Failed to initialize frame fences" << std::endl;
        return false;
    }
    
    // Note: CPU instance buffers no longer used - using GPU entity manager only
    
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

    // 5. Record compute command buffer
    functionLoader->vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0);
    
    VkCommandBufferBeginInfo computeBeginInfo{};
    computeBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (functionLoader->vkBeginCommandBuffer(computeCommandBuffers[currentFrame], &computeBeginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin recording compute command buffer!" << std::endl;
        return;
    }
    
    // Using real-time vertex shader movement computation
    
    // Update total simulation time
    totalTime += deltaTime;
    
    
    
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


    // GPU-only rendering - all entities are triangles
    uint32_t gpuEntityCount = gpuEntityManager ? gpuEntityManager->getEntityCount() : 0;
    if (gpuEntityCount > 0) {
        VkBuffer vertexBuffers[] = {resources->getVertexBuffer(), gpuEntityManager->getCurrentEntityBuffer()};
        VkDeviceSize offsets[] = {0, 0};
        functionLoader->vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
        functionLoader->vkCmdBindIndexBuffer(commandBuffer, resources->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        functionLoader->vkCmdDrawIndexed(commandBuffer, resources->getIndexCount(), gpuEntityCount, 0, 0, 0);
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
        gpuEntityManager->uploadPendingEntities();
    }
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


void VulkanRenderer::setEntityPosition(float x, float y, float z) {
    entityPosition = glm::vec3(x, y, z);
}

void VulkanRenderer::updateEntities(const std::vector<std::tuple<glm::vec3, glm::vec4>>& entities) {
    // This method is deprecated - entities should be added directly to GPU entity manager
    if (!entities.empty()) {
        std::cerr << "Warning: updateEntities() called but CPU rendering is disabled. Use GPU entity manager instead." << std::endl;
    }
}

bool VulkanRenderer::validateEntityCapacity(uint32_t entityCount, const char* source) const {
    // CPU rendering is disabled - validation no longer needed
    // GPU entity manager handles its own capacity validation
    return true;
}

bool VulkanRenderer::testBufferOverflowProtection() const {
    std::cout << "\n=== TESTING GPU ENTITY BUFFER STATE ===" << std::endl;
    
    if (!initialized) {
        std::cerr << "Cannot test - renderer not initialized!" << std::endl;
        return false;
    }
    
    bool allTestsPassed = true;
    
    // Test: GPU buffer limits
    std::cout << "GPU entity buffer state..." << std::endl;
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
        std::cerr << "  ✗ GPU entity manager not available!" << std::endl;
        allTestsPassed = false;
    }
    
    std::cout << "\n=== TEST RESULTS ===" << std::endl;
    if (allTestsPassed) {
        std::cout << "✅ GPU entity buffer is working correctly!" << std::endl;
    } else {
        std::cerr << "❌ GPU entity buffer has issues!" << std::endl;
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

