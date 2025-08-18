#include "vulkan_renderer.h"
#include "vulkan/vulkan_function_loader.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_sync.h"
#include "vulkan/resource_context.h"
#include "vulkan/frame_graph.h"
#include "vulkan/nodes/entity_compute_node.h"
#include "vulkan/nodes/entity_graphics_node.h"
#include "vulkan/nodes/swapchain_present_node.h"
#include "vulkan/render_frame_director.h"
#include "vulkan/command_submission_service.h"
#include "vulkan/frame_graph_resource_registry.h"
#include "vulkan/gpu_synchronization_service.h"
#include "vulkan/presentation_surface.h"
#include "vulkan/pipeline_system_manager.h"
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
    
    // Initialize AAA Pipeline System
    pipelineSystem = std::make_unique<PipelineSystemManager>();
    if (!pipelineSystem->initialize(*context)) {
        std::cerr << "Failed to initialize AAA Pipeline System" << std::endl;
        return false;
    }
    
    // Create render pass using new pipeline system
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
    
    // Create descriptor layout using new pipeline system
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
    
    // Initialize GPU entity manager after sync and resource context are created
    gpuEntityManager = std::make_unique<GPUEntityManager>();
    if (!gpuEntityManager->initialize(*context, sync.get(), resourceContext.get())) {
        std::cerr << "Failed to initialize GPU entity manager" << std::endl;
        return false;
    }
    
    // Update graphics descriptor sets with position buffer for compute-based rendering
    if (!resourceContext->updateDescriptorSetsWithPositionBuffer(gpuEntityManager->getPositionBuffer())) {
        std::cerr << "Failed to update descriptor sets with position buffer" << std::endl;
        return false;
    }
    std::cout << "Graphics descriptor sets updated with position buffer" << std::endl;
    
    // Create compute descriptor layout using new pipeline system
    auto computeLayoutSpec = DescriptorLayoutPresets::createEntityComputeLayout();
    VkDescriptorSetLayout computeDescriptorLayout = pipelineSystem->getLayoutManager()->getLayout(computeLayoutSpec);
    if (computeDescriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute descriptor layout" << std::endl;
        return false;
    }
    
    // Create compute descriptor sets for GPU entity manager
    if (!gpuEntityManager->createComputeDescriptorSets(computeDescriptorLayout)) {
        std::cerr << "Failed to create compute descriptor sets" << std::endl;
        return false;
    }
    
    // Initialize movement command processor
    movementCommandProcessor = std::make_unique<MovementCommandProcessor>(gpuEntityManager.get());
    std::cout << "Movement command processor initialized" << std::endl;
    
    // Initialize modular architecture
    if (!initializeModularArchitecture()) {
        std::cerr << "Failed to initialize modular architecture" << std::endl;
        return false;
    }
    
    // Warmup common pipelines for better performance
    pipelineSystem->warmupCommonPipelines();
    
    std::cout << "VulkanRenderer: AAA Pipeline System initialization complete" << std::endl;
    
    initialized = true;
    return true;
}

void VulkanRenderer::cleanup() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        context->getLoader().vkDeviceWaitIdle(context->getDevice());
    }
    
    // Clean up modular architecture
    cleanupModularArchitecture();
    
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
    // New modular architecture
    drawFrameModular();
}

// ========== NEW MODULAR ARCHITECTURE METHODS ==========

bool VulkanRenderer::initializeModularArchitecture() {
    // Initialize frame graph
    frameGraph = std::make_unique<FrameGraph>();
    if (!frameGraph->initialize(*context, sync.get())) {
        std::cerr << "Failed to initialize frame graph" << std::endl;
        return false;
    }
    
    // Initialize resource importer
    resourceRegistry = std::make_unique<FrameGraphResourceRegistry>();
    if (!resourceRegistry->initialize(frameGraph.get(), gpuEntityManager.get())) {
        std::cerr << "Failed to initialize resource importer" << std::endl;
        return false;
    }
    
    // Import resources into frame graph
    if (!resourceRegistry->importEntityResources()) {
        std::cerr << "Failed to import entity resources" << std::endl;
        return false;
    }
    
    // Initialize synchronization manager
    syncService = std::make_unique<GPUSynchronizationService>();
    if (!syncService->initialize(*context)) {
        std::cerr << "Failed to initialize synchronization manager" << std::endl;
        return false;
    }
    
    // Initialize swapchain coordinator
    presentationSurface = std::make_unique<PresentationSurface>();
    if (!presentationSurface->initialize(context.get(), swapchain.get(), pipelineSystem.get(), syncService.get())) {
        std::cerr << "Failed to initialize swapchain coordinator" << std::endl;
        return false;
    }
    
    // Initialize frame orchestrator
    frameDirector = std::make_unique<RenderFrameDirector>();
    if (!frameDirector->initialize(
        context.get(),
        swapchain.get(), 
        pipelineSystem.get(),
        sync.get(),
        resourceContext.get(),
        gpuEntityManager.get(),
        movementCommandProcessor.get(),
        frameGraph.get()
    )) {
        std::cerr << "Failed to initialize frame orchestrator" << std::endl;
        return false;
    }
    
    // Update frame orchestrator with resource IDs
    frameDirector->updateResourceIds(
        resourceRegistry->getEntityBufferId(),
        resourceRegistry->getPositionBufferId(),
        resourceRegistry->getCurrentPositionBufferId(),
        resourceRegistry->getTargetPositionBufferId()
    );
    
    // Initialize queue submission manager
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
    // CRITICAL FIX: Wait for previous frame compute work to complete
    // Use the same fence system that CommandSubmissionService uses (VulkanSync)
    VkFence computeFence = sync->getComputeFences()[currentFrame];
    VkResult waitResult = context->getLoader().vkWaitForFences(
        context->getDevice(), 1, &computeFence, VK_TRUE, UINT64_MAX);
    if (waitResult != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: Failed to wait for compute fence: " << waitResult << std::endl;
        return;
    }
    
    // Wait for graphics fence using existing system
    if (syncService->waitForGraphicsFence(currentFrame) != VK_SUCCESS) {
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
        // Check if swapchain recreation is needed
        if (presentationSurface->isFramebufferResized()) {
            presentationSurface->recreateSwapchain();
        }
        return;
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
        std::cerr << "Frame submission failed" << std::endl;
        return;
    }
    
    // Handle swapchain recreation if needed
    if (submissionResult.swapchainRecreationNeeded || framebufferResized) {
        framebufferResized = false;
        presentationSurface->recreateSwapchain();
    }
    
    // Update frame state
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