#include "vulkan_renderer.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_resources.h"
#include "vulkan/vulkan_sync.h"
#include <iostream>
#include <array>

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
    
    std::cout << "Creating framebuffers..." << std::endl;
    if (!swapchain->createFramebuffers(pipeline->getRenderPass())) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return false;
    }
    std::cout << "Framebuffers created successfully" << std::endl;
    
    resources = std::make_unique<VulkanResources>();
    if (!resources->initialize(context.get())) {
        std::cerr << "Failed to initialize Vulkan resources" << std::endl;
        return false;
    }
    
    std::cout << "Creating uniform buffers..." << std::endl;
    if (!resources->createUniformBuffers()) {
        std::cerr << "Failed to create uniform buffers" << std::endl;
        return false;
    }
    std::cout << "Uniform buffers created successfully" << std::endl;
    
    std::cout << "Creating descriptor pool..." << std::endl;
    if (!resources->createDescriptorPool(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }
    std::cout << "Descriptor pool created successfully" << std::endl;
    
    std::cout << "Creating descriptor sets..." << std::endl;
    if (!resources->createDescriptorSets(pipeline->getDescriptorSetLayout())) {
        std::cerr << "Failed to create descriptor sets" << std::endl;
        return false;
    }
    std::cout << "Descriptor sets created successfully" << std::endl;
    
    sync = std::make_unique<VulkanSync>();
    if (!sync->initialize(context.get())) {
        std::cerr << "Failed to initialize Vulkan sync" << std::endl;
        return false;
    }
    
    loadDrawingFunctions();
    
    std::cout << "Vulkan initialization completed successfully!" << std::endl;
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

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

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
}