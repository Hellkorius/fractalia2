#include "vulkan_renderer.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_resources.h"
#include "vulkan/vulkan_sync.h"
#include "ecs/systems/camera_system.hpp"
#include "ecs/camera_component.hpp"
#include <iostream>
#include <array>
#include <chrono>
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
    if (!swapchain->initialize(context.get(), window)) {
        std::cerr << "Failed to initialize Vulkan swapchain" << std::endl;
        return false;
    }
    
    pipeline = std::make_unique<VulkanPipeline>();
    if (!pipeline->initialize(context.get(), swapchain->getImageFormat())) {
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
    
    if (!resources->createDescriptorPool(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }
    
    if (!resources->createDescriptorSets(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor sets" << std::endl;
        return false;
    }
    
    loadDrawingFunctions();
    
    initialized = true;
    return true;
}

void VulkanRenderer::cleanup() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        context->vkDeviceWaitIdle(context->getDevice());
    }
    
    sync.reset();
    resources.reset();
    pipeline.reset();
    swapchain.reset();
    context.reset();
    
    initialized = false;
}

void VulkanRenderer::drawFrame() {

    const auto& fences = sync->getInFlightFences();
    const auto& imageAvailableSemaphores = sync->getImageAvailableSemaphores();
    const auto& renderFinishedSemaphores = sync->getRenderFinishedSemaphores();
    const auto& commandBuffers = sync->getCommandBuffers();

    vkWaitForFences(context->getDevice(), 1, &fences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context->getDevice(), swapchain->getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Failed to acquire swap chain image" << std::endl;
        return;
    }

    vkResetFences(context->getDevice(), 1, &fences[currentFrame]);

    updateUniformBuffer(currentFrame);
    updateInstanceBuffer(currentFrame);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, fences[currentFrame]) != VK_SUCCESS) {
        std::cerr << "Failed to submit draw command buffer" << std::endl;
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(context->getPresentQueue(), &presentInfo);

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
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin recording command buffer" << std::endl;
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

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getGraphicsPipeline());
    
    const auto& descriptorSets = resources->getDescriptorSets();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchain->getExtent().width;
    viewport.height = (float) swapchain->getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    // Render all entities by type
    uint32_t instanceOffset = 0;
    
    // Count entities by type
    uint32_t triangleCount = 0;
    uint32_t squareCount = 0;
    for (const auto& [pos, shapeType, color] : renderEntities) {
        if (shapeType == ShapeType::Triangle) triangleCount++;
        else if (shapeType == ShapeType::Square) squareCount++;
    }
    
    
    // Render triangles
    if (triangleCount > 0) {
        VkBuffer triangleVertexBuffers[] = {resources->getTriangleVertexBuffer(), resources->getInstanceBuffers()[currentFrame]};
        VkDeviceSize offsets[] = {0, instanceOffset * sizeof(glm::mat4)};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, triangleVertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, resources->getTriangleIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(commandBuffer, resources->getTriangleIndexCount(), triangleCount, 0, 0, 0);
        instanceOffset += triangleCount;
    }
    
	// Render squares
	if (squareCount > 0) {
		VkBuffer squareVertexBuffers[] = {resources->getSquareVertexBuffer(), resources->getInstanceBuffers()[currentFrame]};
		VkDeviceSize offsets[] = {0, instanceOffset * sizeof(glm::mat4)};
		vkCmdBindVertexBuffers(commandBuffer, 0, 2, squareVertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, resources->getSquareIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(commandBuffer, resources->getSquareIndexCount(), squareCount, 0, 0, 0);
		instanceOffset += squareCount;
	}

    // Emergency test: Draw a simple triangle without instancing
    if (triangleCount == 0 && squareCount == 0) {
        VkBuffer testVertexBuffer = resources->getTriangleVertexBuffer();
        VkDeviceSize testOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &testVertexBuffer, &testOffset);
        vkCmdBindIndexBuffer(commandBuffer, resources->getTriangleIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(commandBuffer, resources->getTriangleIndexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to record command buffer" << std::endl;
    }
}

void VulkanRenderer::loadDrawingFunctions() {
    vkWaitForFences = (PFN_vkWaitForFences)context->vkGetDeviceProcAddr(context->getDevice(), "vkWaitForFences");
    vkResetFences = (PFN_vkResetFences)context->vkGetDeviceProcAddr(context->getDevice(), "vkResetFences");
    vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)context->vkGetDeviceProcAddr(context->getDevice(), "vkAcquireNextImageKHR");
    vkQueueSubmit = (PFN_vkQueueSubmit)context->vkGetDeviceProcAddr(context->getDevice(), "vkQueueSubmit");
    vkQueuePresentKHR = (PFN_vkQueuePresentKHR)context->vkGetDeviceProcAddr(context->getDevice(), "vkQueuePresentKHR");
    vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkBeginCommandBuffer");
    vkEndCommandBuffer = (PFN_vkEndCommandBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkEndCommandBuffer");
    vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdBeginRenderPass");
    vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdEndRenderPass");
    vkCmdBindPipeline = (PFN_vkCmdBindPipeline)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdBindPipeline");
    vkCmdSetViewport = (PFN_vkCmdSetViewport)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdSetViewport");
    vkCmdSetScissor = (PFN_vkCmdSetScissor)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdSetScissor");
    vkCmdDraw = (PFN_vkCmdDraw)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdDraw");
    vkResetCommandBuffer = (PFN_vkResetCommandBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkResetCommandBuffer");
    vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdBindDescriptorSets");
    vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdBindVertexBuffers");
    vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdBindIndexBuffer");
    vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)context->vkGetDeviceProcAddr(context->getDevice(), "vkCmdDrawIndexed");
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
    glm::mat4* matrices = static_cast<glm::mat4*>(data);
    
    uint32_t instanceIndex = 0;
    
    // Process triangles first (to match render order)
    for (const auto& [pos, shapeType, color] : renderEntities) {
        if (shapeType == ShapeType::Triangle) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::rotate(model, time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            matrices[instanceIndex++] = model;
        }
    }
    
    // Process squares second (to match render order)
    for (const auto& [pos, shapeType, color] : renderEntities) {
        if (shapeType == ShapeType::Square) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::rotate(model, time * glm::radians(-30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            matrices[instanceIndex++] = model;
        }
    }
}

void VulkanRenderer::setEntityPosition(float x, float y, float z) {
    entityPosition = glm::vec3(x, y, z);
}

void VulkanRenderer::updateEntities(const std::vector<std::tuple<glm::vec3, ShapeType, glm::vec4>>& entities) {
    renderEntities = entities;
}

void VulkanRenderer::updateAspectRatio(int windowWidth, int windowHeight) {
    if (world != nullptr) {
        CameraManager::updateAspectRatio(*world, windowWidth, windowHeight);
    }
}