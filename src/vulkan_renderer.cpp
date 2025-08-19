#include "vulkan_renderer.h"
#include "vulkan/core/vulkan_function_loader.h"
#include "vulkan/core/vulkan_context.h"
#include "vulkan/core/vulkan_swapchain.h"
#include "vulkan/core/vulkan_sync.h"
#include "vulkan/core/queue_manager.h"
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
    if (!window) {
        std::cerr << "VulkanRenderer: NULL window provided" << std::endl;
        return false;
    }
    this->window = window;
    
    // Phase 1: Core Vulkan initialization
    context = std::make_unique<VulkanContext>();
    if (!context || !context->initialize(window)) {
        std::cerr << "Failed to initialize Vulkan context" << std::endl;
        cleanup();
        return false;
    }
    
    swapchain = std::make_unique<VulkanSwapchain>();
    if (!swapchain || !swapchain->initialize(*context, window)) {
        std::cerr << "Failed to initialize Vulkan swapchain" << std::endl;
        cleanup();
        return false;
    }
    
    // Phase 2: Pipeline and synchronization objects (depend on context)
    pipelineSystem = std::make_unique<PipelineSystemManager>();
    if (!pipelineSystem || !pipelineSystem->initialize(*context)) {
        std::cerr << "Failed to initialize AAA Pipeline System" << std::endl;
        cleanup();
        return false;
    }
    
    sync = std::make_unique<VulkanSync>();
    if (!sync || !sync->initialize(*context)) {
        std::cerr << "Failed to initialize Vulkan sync" << std::endl;
        cleanup();
        return false;
    }
    
    queueManager = std::make_unique<QueueManager>();
    if (!queueManager || !queueManager->initialize(*context)) {
        std::cerr << "Failed to initialize Queue Manager" << std::endl;
        cleanup();
        return false;
    }
    
    // Phase 3: Render pass and framebuffers (depend on pipeline system and swapchain)
    if (!pipelineSystem->getGraphicsManager()) {
        std::cerr << "Graphics manager not available from pipeline system" << std::endl;
        cleanup();
        return false;
    }
    
    VkRenderPass renderPass = pipelineSystem->getGraphicsManager()->createRenderPass(
        swapchain->getImageFormat(), VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_2_BIT, true);
    if (renderPass == VK_NULL_HANDLE) {
        std::cerr << "Failed to create render pass" << std::endl;
        cleanup();
        return false;
    }
    
    if (!swapchain->createFramebuffers(renderPass)) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        cleanup();
        return false;
    }
    
    // Phase 4: Resource management (depends on context, queue manager)
    resourceContext = std::make_unique<ResourceContext>();
    if (!resourceContext || !resourceContext->initialize(*context, queueManager.get())) {
        std::cerr << "Failed to initialize Resource context" << std::endl;
        cleanup();
        return false;
    }
    
    if (!resourceContext->createUniformBuffers()) {
        std::cerr << "Failed to create uniform buffers" << std::endl;
        cleanup();
        return false;
    }
    
    if (!resourceContext->createTriangleBuffers()) {
        std::cerr << "Failed to create triangle buffers" << std::endl;
        cleanup();
        return false;
    }
    
    // Phase 5: Descriptor layouts and pools (depend on pipeline system)
    if (!pipelineSystem->getLayoutManager()) {
        std::cerr << "Layout manager not available from pipeline system" << std::endl;
        cleanup();
        return false;
    }
    
    auto layoutSpec = DescriptorLayoutPresets::createEntityGraphicsLayout();
    VkDescriptorSetLayout descriptorLayout = pipelineSystem->getLayoutManager()->getLayout(layoutSpec);
    if (descriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create descriptor layout" << std::endl;
        cleanup();
        return false;
    }
    
    if (!resourceContext->createGraphicsDescriptorPool(descriptorLayout)) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        cleanup();
        return false;
    }
    
    if (!resourceContext->createGraphicsDescriptorSets(descriptorLayout)) {
        std::cerr << "Failed to create descriptor sets" << std::endl;
        cleanup();
        return false;
    }
    
    // Phase 6: Entity management (depends on context, sync, resource context)
    gpuEntityManager = std::make_unique<GPUEntityManager>();
    if (!gpuEntityManager || !gpuEntityManager->initialize(*context, sync.get(), resourceContext.get())) {
        std::cerr << "Failed to initialize GPU entity manager" << std::endl;
        cleanup();
        return false;
    }
    
    // Validate entity manager buffers before using them
    if (!gpuEntityManager->getEntityBuffer() || !gpuEntityManager->getPositionBuffer()) {
        std::cerr << "GPU entity manager missing required buffers" << std::endl;
        cleanup();
        return false;
    }
    
    if (!resourceContext->updateDescriptorSetsWithEntityAndPositionBuffers(
            gpuEntityManager->getEntityBuffer(),
            gpuEntityManager->getPositionBuffer())) {
        std::cerr << "Failed to update descriptor sets with entity and position buffers" << std::endl;
        cleanup();
        return false;
    }
    std::cout << "Graphics descriptor sets updated with entity and position buffers" << std::endl;
    
    auto computeLayoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout computeDescriptorLayout = pipelineSystem->getLayoutManager()->getLayout(computeLayoutSpec);
    if (computeDescriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute descriptor layout" << std::endl;
        cleanup();
        return false;
    }
    
    if (!gpuEntityManager->createComputeDescriptorSets(computeDescriptorLayout)) {
        std::cerr << "Failed to create compute descriptor sets" << std::endl;
        cleanup();
        return false;
    }
    
    // Phase 7: Command processing (depends on GPU entity manager)
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    if (!movementCommandProcessor) {
        std::cerr << "Failed to create movement command processor" << std::endl;
        cleanup();
        return false;
    }
    std::cout << "Movement command processor initialized" << std::endl;
    
    // Phase 8: Modular architecture (depends on all previous components)
    if (!initializeModularArchitecture()) {
        std::cerr << "Failed to initialize modular architecture" << std::endl;
        cleanup();
        return false;
    }
    
    // Final validation - ensure all critical components are available
    if (!validateInitializationState()) {
        std::cerr << "Initialization state validation failed" << std::endl;
        cleanup();
        return false;
    }
    
    pipelineSystem->warmupCommonPipelines();
    
    std::cout << "VulkanRenderer: AAA Pipeline System initialization complete" << std::endl;
    
    initialized = true;
    return true;
}

void VulkanRenderer::cleanup() {
    // Wait for device to be idle before cleanup - but only if context is valid
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        try {
            context->getLoader().vkDeviceWaitIdle(context->getDevice());
        } catch (const std::exception& e) {
            std::cerr << "Exception during device wait idle: " << e.what() << std::endl;
        }
    }
    
    // Cleanup modular architecture first (higher-level components)
    cleanupModularArchitecture();
    
    // Cleanup RAII resources before destroying their dependencies
    if (sync) {
        try {
            sync->cleanupBeforeContextDestruction();
        } catch (const std::exception& e) {
            std::cerr << "Exception during sync cleanup: " << e.what() << std::endl;
        }
    }
    
    if (pipelineSystem) {
        try {
            pipelineSystem->cleanupBeforeContextDestruction();
        } catch (const std::exception& e) {
            std::cerr << "Exception during pipeline system cleanup: " << e.what() << std::endl;
        }
    }
    
    // Reset components in reverse dependency order
    if (movementCommandProcessor) {
        movementCommandProcessor.reset();
    }
    
    if (gpuEntityManager) {
        gpuEntityManager.reset();
    }
    
    if (resourceContext) {
        resourceContext.reset();
    }
    
    if (queueManager) {
        queueManager.reset();
    }
    
    if (sync) {
        sync.reset();
    }
    
    if (pipelineSystem) {
        pipelineSystem.reset();
    }
    
    if (swapchain) {
        swapchain.reset();
    }
    
    if (context) {
        context.reset();
    }
    
    initialized = false;
    window = nullptr;
}

void VulkanRenderer::drawFrame() {
    drawFrameModular();
}


bool VulkanRenderer::initializeModularArchitecture() {
    frameGraph = std::make_unique<FrameGraph>();
    if (!frameGraph->initialize(*context, sync.get(), queueManager.get())) {
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
    if (!submissionService->initialize(context.get(), sync.get(), swapchain.get(), queueManager.get())) {
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
        
        // Proactive swapchain recreation - check for common failure conditions
        bool shouldRecreate = false;
        const char* recreationReason = nullptr;
        
        if (presentationSurface && presentationSurface->isFramebufferResized() && !recreationInProgress) {
            shouldRecreate = true;
            recreationReason = "framebuffer resize detected";
        } else {
            // For now, attempt recreation on any frame failure as a fallback
            shouldRecreate = true;
            recreationReason = "general frame failure (proactive recovery)";
        }
        
        if (shouldRecreate) {
            std::cout << "VulkanRenderer: Initiating proactive swapchain recreation due to: " << recreationReason << std::endl;
            if (attemptSwapchainRecreation()) {
                std::cout << "VulkanRenderer: Swapchain recreation successful, retrying frame" << std::endl;
                // Retry the frame immediately after successful recreation
                if (frameDirector) {
                    frameResult = frameDirector->directFrame(currentFrame, totalTime, deltaTime, frameCounter, world);
                    if (frameResult.success) {
                        std::cout << "VulkanRenderer: Frame retry after swapchain recreation succeeded" << std::endl;
                    } else {
                        std::cerr << "VulkanRenderer: Frame retry after swapchain recreation still failed" << std::endl;
                        return;
                    }
                } else {
                    std::cerr << "VulkanRenderer: Frame director unavailable for retry" << std::endl;
                    return;
                }
            } else {
                std::cerr << "VulkanRenderer: Swapchain recreation failed, skipping frame" << std::endl;
                return;
            }
        } else {
            std::cerr << "VulkanRenderer: Frame failure not related to swapchain, skipping frame" << std::endl;
            return;
        }
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
        
        if (attemptSwapchainRecreation()) {
            std::cout << "VulkanRenderer: SWAPCHAIN RECREATION COMPLETED - Next frames should render normally" << std::endl;
        } else {
            std::cerr << "VulkanRenderer: CRITICAL ERROR - Swapchain recreation FAILED" << std::endl;
        }
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

bool VulkanRenderer::validateInitializationState() const {
    // Validate core Vulkan components
    if (!context || !context->getDevice()) {
        std::cerr << "VulkanRenderer: Invalid Vulkan context or device" << std::endl;
        return false;
    }
    
    if (!swapchain) {
        std::cerr << "VulkanRenderer: Missing swapchain" << std::endl;
        return false;
    }
    
    if (!pipelineSystem) {
        std::cerr << "VulkanRenderer: Missing pipeline system" << std::endl;
        return false;
    }
    
    if (!sync) {
        std::cerr << "VulkanRenderer: Missing synchronization objects" << std::endl;
        return false;
    }
    
    if (!queueManager) {
        std::cerr << "VulkanRenderer: Missing queue manager" << std::endl;
        return false;
    }
    
    if (!resourceContext) {
        std::cerr << "VulkanRenderer: Missing resource context" << std::endl;
        return false;
    }
    
    if (!gpuEntityManager) {
        std::cerr << "VulkanRenderer: Missing GPU entity manager" << std::endl;
        return false;
    }
    
    // Validate modular architecture components
    if (!frameGraph) {
        std::cerr << "VulkanRenderer: Missing frame graph" << std::endl;
        return false;
    }
    
    if (!frameDirector) {
        std::cerr << "VulkanRenderer: Missing frame director" << std::endl;
        return false;
    }
    
    if (!submissionService) {
        std::cerr << "VulkanRenderer: Missing submission service" << std::endl;
        return false;
    }
    
    if (!presentationSurface) {
        std::cerr << "VulkanRenderer: Missing presentation surface" << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanRenderer::attemptSwapchainRecreation() {
    if (!presentationSurface || recreationInProgress) {
        return false;
    }
    
    recreationInProgress = true;
    
    try {
        // Wait for device to be idle before recreation
        if (context && context->getDevice() != VK_NULL_HANDLE) {
            context->getLoader().vkDeviceWaitIdle(context->getDevice());
        }
        
        bool recreationSuccess = presentationSurface->recreateSwapchain();
        if (recreationSuccess && frameDirector) {
            frameDirector->resetSwapchainCache();
            std::cout << "VulkanRenderer: Swapchain recreation and cache reset successful" << std::endl;
        } else {
            std::cerr << "VulkanRenderer: Swapchain recreation failed" << std::endl;
            recreationInProgress = false;
            return false;
        }
        
        framebufferResized = false;
        recreationInProgress = false;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "VulkanRenderer: Exception during swapchain recreation: " << e.what() << std::endl;
        recreationInProgress = false;
        return false;
    }
}