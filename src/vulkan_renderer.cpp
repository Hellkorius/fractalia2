#include "vulkan_renderer.h"
#include "vulkan/core/vulkan_function_loader.h"
#include "vulkan/core/vulkan_context.h"
#include "vulkan/core/vulkan_swapchain.h"
#include "vulkan/core/vulkan_sync.h"
#include "vulkan/resources/resource_context.h"
#include "vulkan/rendering/frame_graph.h"
#include "vulkan/nodes/entity_compute_node.h"
#include "vulkan/nodes/entity_graphics_node.h"
#include "vulkan/nodes/swapchain_present_node.h"
#include "vulkan/services/render_frame_director.h"
#include "vulkan/services/command_submission_service.h"
#include "vulkan/rendering/frame_graph_resource_registry.h"
#include "vulkan/services/gpu_synchronization_service.h"
#include "vulkan/services/presentation_surface.h"
#include "vulkan/pipelines/pipeline_system_manager.h"
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
    
    pipelineSystem = std::make_unique<PipelineSystemManager>();
    if (!pipelineSystem->initialize(*context)) {
        std::cerr << "Failed to initialize AAA Pipeline System" << std::endl;
        return false;
    }
    
    VkRenderPass renderPass = pipelineSystem->getGraphicsManager()->createRenderPass(
        swapchain->getImageFormat(), VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_2_BIT, true);
    if (renderPass == VK_NULL_HANDLE) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }
    
    if (!swapchain->createFramebuffers(renderPass)) {
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
    
    auto layoutSpec = DescriptorLayoutPresets::createEntityGraphicsLayout();
    VkDescriptorSetLayout descriptorLayout = pipelineSystem->getLayoutManager()->getLayout(layoutSpec);
    if (descriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create descriptor layout" << std::endl;
        return false;
    }
    
    if (!resourceContext->createGraphicsDescriptorPool(descriptorLayout)) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }
    
    if (!resourceContext->createGraphicsDescriptorSets(descriptorLayout)) {
        std::cerr << "Failed to create descriptor sets" << std::endl;
        return false;
    }
    
    gpuEntityManager = std::make_unique<GPUEntityManager>();
    if (!gpuEntityManager->initialize(*context, sync.get(), resourceContext.get())) {
        std::cerr << "Failed to initialize GPU entity manager" << std::endl;
        return false;
    }
    
    if (!resourceContext->updateDescriptorSetsWithEntityAndPositionBuffers(
            gpuEntityManager->getEntityBuffer(),
            gpuEntityManager->getPositionBuffer())) {
        std::cerr << "Failed to update descriptor sets with entity and position buffers" << std::endl;
        return false;
    }
    std::cout << "Graphics descriptor sets updated with entity and position buffers" << std::endl;
    
    auto computeLayoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout computeDescriptorLayout = pipelineSystem->getLayoutManager()->getLayout(computeLayoutSpec);
    if (computeDescriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute descriptor layout" << std::endl;
        return false;
    }
    
    if (!gpuEntityManager->createComputeDescriptorSets(computeDescriptorLayout)) {
        std::cerr << "Failed to create compute descriptor sets" << std::endl;
        return false;
    }
    
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    std::cout << "Movement command processor initialized" << std::endl;
    
    if (!initializeModularArchitecture()) {
        std::cerr << "Failed to initialize modular architecture" << std::endl;
        return false;
    }
    
    pipelineSystem->warmupCommonPipelines();
    
    std::cout << "VulkanRenderer: AAA Pipeline System initialization complete" << std::endl;
    
    initialized = true;
    return true;
}

void VulkanRenderer::cleanup() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        context->getLoader().vkDeviceWaitIdle(context->getDevice());
    }
    
    cleanupModularArchitecture();
    
    if (sync) {
        sync->cleanupBeforeContextDestruction();
    }
    if (pipelineSystem) {
        pipelineSystem->cleanupBeforeContextDestruction();
    }
    
    movementCommandProcessor.reset();
    gpuEntityManager.reset();
    resourceContext.reset();
    sync.reset();
    pipelineSystem.reset();
    swapchain.reset();
    context.reset();
    
    initialized = false;
}

void VulkanRenderer::drawFrame() {
    drawFrameModular();
}


bool VulkanRenderer::initializeModularArchitecture() {
    frameGraph = std::make_unique<FrameGraph>();
    if (!frameGraph->initialize(*context, sync.get())) {
        std::cerr << "Failed to initialize frame graph" << std::endl;
        return false;
    }
    
    resourceRegistry = std::make_unique<FrameGraphResourceRegistry>();
    if (!resourceRegistry->initialize(frameGraph.get(), gpuEntityManager.get())) {
        std::cerr << "Failed to initialize resource importer" << std::endl;
        return false;
    }
    
    if (!resourceRegistry->importEntityResources()) {
        std::cerr << "Failed to import entity resources" << std::endl;
        return false;
    }
    
    syncService = std::make_unique<GPUSynchronizationService>();
    if (!syncService->initialize(*context)) {
        std::cerr << "Failed to initialize synchronization manager" << std::endl;
        return false;
    }
    
    presentationSurface = std::make_unique<PresentationSurface>();
    if (!presentationSurface->initialize(context.get(), swapchain.get(), pipelineSystem.get(), syncService.get())) {
        std::cerr << "Failed to initialize swapchain coordinator" << std::endl;
        return false;
    }
    
    frameDirector = std::make_unique<RenderFrameDirector>();
    if (!frameDirector->initialize(
        context.get(),
        swapchain.get(), 
        pipelineSystem.get(),
        sync.get(),
        resourceContext.get(),
        gpuEntityManager.get(),
        movementCommandProcessor.get(),
        frameGraph.get(),
        presentationSurface.get()
    )) {
        std::cerr << "Failed to initialize frame orchestrator" << std::endl;
        return false;
    }
    
    frameDirector->updateResourceIds(
        resourceRegistry->getEntityBufferId(),
        resourceRegistry->getPositionBufferId(),
        resourceRegistry->getCurrentPositionBufferId(),
        resourceRegistry->getTargetPositionBufferId()
    );
    
    submissionService = std::make_unique<CommandSubmissionService>();
    if (!submissionService->initialize(context.get(), sync.get(), swapchain.get())) {
        std::cerr << "Failed to initialize queue submission manager" << std::endl;
        return false;
    }
    
    std::cout << "Modular architecture initialized successfully" << std::endl;
    return true;
}

void VulkanRenderer::cleanupModularArchitecture() {
    submissionService.reset();
    frameDirector.reset();
    presentationSurface.reset();
    syncService.reset();
    resourceRegistry.reset();
    frameGraph.reset();
}

void VulkanRenderer::drawFrameModular() {
    // Wait for previous frame GPU work to complete
    // Use VulkanSync fences directly (same system CommandSubmissionService uses)
    VkFence computeFence = sync->getComputeFence(currentFrame);
    VkFence graphicsFence = sync->getInFlightFence(currentFrame);
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    // Wait for both compute and graphics fences in a single call for efficiency
    VkFence fences[] = {computeFence, graphicsFence};
    VkResult waitResult = vk.vkWaitForFences(device, 2, fences, VK_TRUE, UINT64_MAX);
    if (waitResult != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: Failed to wait for GPU fences: " << waitResult << std::endl;
        return;
    }
    
    // Upload pending GPU entities
    uploadPendingGPUEntities();
    
    // Orchestrate the frame
    auto frameResult = frameDirector->directFrame(
        currentFrame,
        totalTime,
        deltaTime, 
        frameCounter,
        world
    );
    
    if (!frameResult.success) {
        std::cerr << "VulkanRenderer: Frame " << frameCounter << " FAILED in frameDirector->directFrame()" << std::endl;
        if (presentationSurface->isFramebufferResized() && !recreationInProgress) {
            std::cout << "VulkanRenderer: Attempting swapchain recreation due to frame failure" << std::endl;
            recreationInProgress = true;
            presentationSurface->recreateSwapchain();
            frameDirector->resetSwapchainCache();
            recreationInProgress = false;
        }
        return;
    } else {
        // DETAILED LOGGING: Monitor first few frames after resize
        static uint32_t lastRecreationFrame = 0;
        static uint32_t framesAfterRecreation = 0;
        
        if (recreationInProgress || (frameCounter - lastRecreationFrame <= 10 && lastRecreationFrame > 0)) {
            if (recreationInProgress) {
                lastRecreationFrame = frameCounter;
                framesAfterRecreation = 0;
            } else {
                framesAfterRecreation = frameCounter - lastRecreationFrame;
            }
            
            std::cout << "VulkanRenderer: Frame " << frameCounter << " SUCCESS (+" << framesAfterRecreation 
                     << " frames post-resize) - Frame direction completed successfully" << std::endl;
        }
    }
    
    // Note: Frame graph nodes already configured in directFrame() - no need to configure again
    
    // Submit frame work
    auto submissionResult = submissionService->submitFrame(
        currentFrame,
        frameResult.imageIndex,
        frameResult.executionResult,
        framebufferResized
    );
    
    if (!submissionResult.success) {
        std::cerr << "VulkanRenderer: Frame " << frameCounter << " FAILED in submissionService->submitFrame()" << std::endl;
        std::cerr << "  VkResult: " << submissionResult.lastResult << std::endl;
        return;
    } else {
        // DETAILED LOGGING: Monitor first few frames after resize for submission success
        static uint32_t lastRecreationFrame2 = 0;
        static uint32_t framesAfterRecreation2 = 0;
        
        if (recreationInProgress || (frameCounter - lastRecreationFrame2 <= 10 && lastRecreationFrame2 > 0)) {
            if (recreationInProgress) {
                lastRecreationFrame2 = frameCounter;
                framesAfterRecreation2 = 0;
            } else {
                framesAfterRecreation2 = frameCounter - lastRecreationFrame2;
            }
            
            std::cout << "VulkanRenderer: Frame " << frameCounter << " SUCCESS (+" << framesAfterRecreation2 
                     << " frames post-resize) - Frame submission completed successfully" << std::endl;
        }
    }
    
    if ((submissionResult.swapchainRecreationNeeded || framebufferResized) && !recreationInProgress) {
        std::cout << "VulkanRenderer: SWAPCHAIN RECREATION INITIATED - Frame " << frameCounter << std::endl;
        recreationInProgress = true;
        framebufferResized = false;
        
        bool recreationSuccess = presentationSurface->recreateSwapchain();
        if (recreationSuccess) {
            std::cout << "VulkanRenderer: Swapchain recreation SUCCESSFUL" << std::endl;
            frameDirector->resetSwapchainCache();
            std::cout << "VulkanRenderer: Swapchain cache reset SUCCESSFUL" << std::endl;
        } else {
            std::cerr << "VulkanRenderer: CRITICAL ERROR - Swapchain recreation FAILED" << std::endl;
        }
        
        recreationInProgress = false;
        std::cout << "VulkanRenderer: SWAPCHAIN RECREATION COMPLETED - Next frames should render normally" << std::endl;
    }
    
    totalTime += deltaTime;
    frameCounter++;
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ========== LEGACY METHOD IMPLEMENTATIONS ==========

void VulkanRenderer::uploadPendingGPUEntities() {
    if (gpuEntityManager && gpuEntityManager->hasPendingUploads()) {
        gpuEntityManager->uploadPendingEntities();
    }
}

bool VulkanRenderer::validateEntityCapacity(uint32_t entityCount, const char* source) const {
    if (!gpuEntityManager) return false;
    
    uint32_t maxCapacity = gpuEntityManager->getMaxEntities();
    if (entityCount > maxCapacity) {
        std::cerr << "VulkanRenderer: " << source << " requested " << entityCount 
                  << " entities, but max capacity is " << maxCapacity << std::endl;
        return false;
    }
    return true;
}

bool VulkanRenderer::testBufferOverflowProtection() const {
    if (!gpuEntityManager) return false;
    
    uint32_t gpuEntityCount = gpuEntityManager->getEntityCount();
    uint32_t maxGpuEntities = gpuEntityManager->getMaxEntities();
    
    std::cout << "Buffer overflow protection test:" << std::endl;
    std::cout << "GPU entities: " << gpuEntityCount << "/" << maxGpuEntities << std::endl;
    
    return gpuEntityCount <= maxGpuEntities;
}

void VulkanRenderer::updateAspectRatio(int windowWidth, int windowHeight) {
    if (world) {
        CameraManager::updateAspectRatio(*world, windowWidth, windowHeight);
    }
}

void VulkanRenderer::setFramebufferResized(bool resized) {
    if (presentationSurface) {
        presentationSurface->setFramebufferResized(resized);
    }
}