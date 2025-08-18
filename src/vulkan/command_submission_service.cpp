#include "command_submission_service.h"
#include "vulkan_context.h"
#include "vulkan_sync.h"
#include "vulkan_swapchain.h"
#include "vulkan_function_loader.h"
#include <iostream>

CommandSubmissionService::CommandSubmissionService() {
}

CommandSubmissionService::~CommandSubmissionService() {
    cleanup();
}

bool CommandSubmissionService::initialize(VulkanContext* context, VulkanSync* sync, VulkanSwapchain* swapchain) {
    this->context = context;
    this->sync = sync;
    this->swapchain = swapchain;
    return true;
}

void CommandSubmissionService::cleanup() {
    // Dependencies are managed externally
}

SubmissionResult CommandSubmissionService::submitFrame(
    uint32_t currentFrame,
    uint32_t imageIndex,
    const FrameGraph::ExecutionResult& executionResult,
    bool framebufferResized
) {
    SubmissionResult result;

    // ASYNC COMPUTE: Submit compute and graphics work in parallel
    // Compute calculates frame N+1 while graphics renders frame N
    
    // 1. Submit compute work asynchronously (no waiting for graphics)
    if (executionResult.computeCommandBufferUsed) {
        result = submitComputeWorkAsync(currentFrame + 1); // Compute for NEXT frame
        if (!result.success) {
            return result;
        }
    }

    // 2. Submit graphics work in parallel (uses previous frame's compute results)
    if (executionResult.graphicsCommandBufferUsed) {
        result = submitGraphicsWork(currentFrame);
        if (!result.success) {
            return result;
        }

        // 3. Present frame
        result = presentFrame(currentFrame, imageIndex, framebufferResized);
    }

    return result;
}

SubmissionResult CommandSubmissionService::submitComputeWorkAsync(uint32_t computeFrame) {
    SubmissionResult result;

    // Use current frame index for command buffer selection (not computeFrame)
    uint32_t frameIndex = computeFrame % sync->getComputeCommandBuffers().size();
    const auto& computeCommandBuffers = sync->getComputeCommandBuffers();
    VkCommandBuffer computeCommandBuffer = computeCommandBuffers[frameIndex];

    // Reset compute fence for this frame
    VkFence computeFence = sync->getComputeFences()[frameIndex];
    VkResult resetResult = context->getLoader().vkResetFences(context->getDevice(), 1, &computeFence);
    if (resetResult != VK_SUCCESS) {
        std::cerr << "CommandSubmissionService: Failed to reset compute fence: " << resetResult << std::endl;
        result.lastResult = resetResult;
        return result;
    }

    // Submit compute work with semaphore signaling for multi-queue synchronization
    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;
    
    // ASYNC COMPUTE: No semaphore signaling needed since compute works on frame N+1
    // Graphics reads from frame N-1 buffer, so no synchronization required

    VkResult computeSubmitResult = context->getLoader().vkQueueSubmit(
        context->getComputeQueue(), 
        1, 
        &computeSubmitInfo, 
        computeFence
    );

    if (computeSubmitResult != VK_SUCCESS) {
        std::cerr << "CommandSubmissionService: Failed to submit compute commands: " << computeSubmitResult << std::endl;
        result.lastResult = computeSubmitResult;
        return result;
    }
    result.success = true;
    return result;
}

SubmissionResult CommandSubmissionService::submitGraphicsWork(uint32_t currentFrame) {
    SubmissionResult result;

    const auto& commandBuffers = sync->getCommandBuffers();
    VkCommandBuffer graphicsCommandBuffer = commandBuffers[currentFrame];

    // Reset graphics fence
    VkFence graphicsFence = sync->getInFlightFences()[currentFrame];
    VkResult resetResult = context->getLoader().vkResetFences(context->getDevice(), 1, &graphicsFence);
    if (resetResult != VK_SUCCESS) {
        std::cerr << "CommandSubmissionService: Failed to reset graphics fence: " << resetResult << std::endl;
        result.lastResult = resetResult;
        return result;
    }

    // Setup graphics submission - async compute model (no compute sync needed)
    VkSubmitInfo graphicsSubmitInfo{};
    graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Only wait for swapchain image availability (async compute works on different buffer)
    VkSemaphore waitSemaphores[] = {sync->getImageAvailableSemaphores()[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    graphicsSubmitInfo.waitSemaphoreCount = 1;
    graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
    graphicsSubmitInfo.pWaitDstStageMask = waitStages;
    graphicsSubmitInfo.commandBufferCount = 1;
    graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffer;

    VkSemaphore signalSemaphores[] = {sync->getRenderFinishedSemaphores()[currentFrame]};
    graphicsSubmitInfo.signalSemaphoreCount = 1;
    graphicsSubmitInfo.pSignalSemaphores = signalSemaphores;

    VkResult graphicsSubmitResult = context->getLoader().vkQueueSubmit(
        context->getGraphicsQueue(), 
        1, 
        &graphicsSubmitInfo, 
        graphicsFence
    );

    if (graphicsSubmitResult != VK_SUCCESS) {
        std::cerr << "CommandSubmissionService: Failed to submit graphics commands: " << graphicsSubmitResult << std::endl;
        result.lastResult = graphicsSubmitResult;
        return result;
    }
    result.success = true;
    return result;
}

SubmissionResult CommandSubmissionService::presentFrame(uint32_t currentFrame, uint32_t imageIndex, bool framebufferResized) {
    SubmissionResult result;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    
    VkSemaphore signalSemaphores[] = {sync->getRenderFinishedSemaphores()[currentFrame]};
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = context->getLoader().vkQueuePresentKHR(context->getPresentQueue(), &presentInfo);
    
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
        result.swapchainRecreationNeeded = true;
        result.success = true; // Still successful, just needs recreation
    } else if (presentResult != VK_SUCCESS) {
        std::cerr << "CommandSubmissionService: Failed to present swap chain image: " << presentResult << std::endl;
        result.lastResult = presentResult;
        return result;
    } else {
        result.success = true;
    }

    result.lastResult = presentResult;
    return result;
}