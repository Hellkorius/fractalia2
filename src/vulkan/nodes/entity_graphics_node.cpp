#include "entity_graphics_node.h"
#include "../pipelines/graphics_pipeline_manager.h"
#include "../core/vulkan_swapchain.h"
#include "../resources/core/resource_coordinator.h"
#include "../resources/managers/graphics_resource_manager.h"
#include "../../ecs/gpu/gpu_entity_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../../ecs/components/camera_component.h"
#include "../pipelines/descriptor_layout_manager.h"
#include <iostream>
#include "../../ecs/core/service_locator.h"
#include "../../ecs/services/camera_service.h"
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>
#include <stdexcept>
#include <memory>

EntityGraphicsNode::EntityGraphicsNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId colorTarget,
    GraphicsPipelineManager* graphicsManager,
    VulkanSwapchain* swapchain,
    ResourceCoordinator* resourceCoordinator,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , colorTargetId(colorTarget)
  , graphicsManager(graphicsManager)
  , swapchain(swapchain)
  , resourceCoordinator(resourceCoordinator)
  , gpuEntityManager(gpuEntityManager) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!graphicsManager) {
        throw std::invalid_argument("EntityGraphicsNode: graphicsManager cannot be null");
    }
    if (!swapchain) {
        throw std::invalid_argument("EntityGraphicsNode: swapchain cannot be null");
    }
    if (!resourceCoordinator) {
        throw std::invalid_argument("EntityGraphicsNode: resourceCoordinator cannot be null");
    }
    if (!gpuEntityManager) {
        throw std::invalid_argument("EntityGraphicsNode: gpuEntityManager cannot be null");
    }
}

std::vector<ResourceDependency> EntityGraphicsNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
        {positionBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
    };
}

std::vector<ResourceDependency> EntityGraphicsNode::getOutputs() const {
    // ELEGANT SOLUTION: Use dynamic swapchain image ID resolved each frame
    return {
        {currentSwapchainImageId, ResourceAccess::Write, PipelineStage::ColorAttachment},
    };
}

void EntityGraphicsNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph, float time, float deltaTime) {
    
    // Validate dependencies are still valid
    if (!graphicsManager || !swapchain || !resourceCoordinator || !gpuEntityManager) {
        std::cerr << "EntityGraphicsNode: Critical error - dependencies became null during execution" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    
    if (entityCount == 0) {
        FRAME_GRAPH_DEBUG_LOG_THROTTLED(noEntitiesCounter, 1800, "EntityGraphicsNode: No entities to render");
        return;
    }
    
    // Get Vulkan context from frame graph
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << "EntityGraphicsNode: Missing Vulkan context" << std::endl;
        return;
    }
    
    // Update uniform buffer with camera matrices (now handled by EntityDescriptorManager)
    updateUniformBuffer();
    
    // Create graphics pipeline state for entity rendering - use Vulkan 1.3 descriptor indexing
    auto layoutSpec = DescriptorLayoutPresets::createEntityIndexedLayout();
    VkDescriptorSetLayout descriptorLayout = graphicsManager->getLayoutManager()->getLayout(layoutSpec);
    
    // Create graphics pipeline state for dynamic rendering (no render pass needed)
    GraphicsPipelineState pipelineState = GraphicsPipelinePresets::createEntityRenderingStateDynamic(
        descriptorLayout, 
        swapchain->getImageFormat(),  // Color format for dynamic rendering
        VK_FORMAT_UNDEFINED,          // No depth format
        VK_SAMPLE_COUNT_2_BIT         // MSAA samples
    );
    
    // Get pipeline and layout
    VkPipeline pipeline = graphicsManager->getPipeline(pipelineState);
    
    VkPipelineLayout pipelineLayout = graphicsManager->getPipelineLayout(pipelineState);
    
    if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
        std::cerr << "EntityGraphicsNode: Failed to get graphics pipeline" << std::endl;
        return;
    }
    
    // Validate swapchain state before accessing image views
    const auto& swapchainImageViews = swapchain->getImageViews();
    if (imageIndex >= swapchainImageViews.size()) {
        std::cerr << "EntityGraphicsNode: Invalid imageIndex " << imageIndex 
                  << " >= swapchain image count " << swapchainImageViews.size() << std::endl;
        return;
    }
    
    // Begin dynamic rendering - setup color and resolve attachments  
    VkImageView msaaColorImageView = swapchain->getMSAAColorImageView();
    
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = msaaColorImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // MSAA is resolved, don't store
    colorAttachment.clearValue.color = {{0.1f, 0.1f, 0.2f, 1.0f}};
    
    // Resolve attachment for MSAA
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = swapchainImageViews[imageIndex];
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = swapchain->getExtent();
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;    // No depth
    renderingInfo.pStencilAttachment = nullptr;  // No stencil
    
    // Cache loader reference for performance
    const auto& vk = context->getLoader();
    
    vk.vkCmdBeginRendering(commandBuffer, &renderingInfo);

    // Set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain->getExtent().width);
    viewport.height = static_cast<float>(swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vk.vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    vk.vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Entity count already retrieved above
    
    // Bind graphics pipeline
    vk.vkCmdBindPipeline(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipeline
    );
    
    // Bind single descriptor set with unified layout (uniform + storage buffers, using Vulkan 1.3 indexing)
    VkDescriptorSet entityDescriptorSet = gpuEntityManager->getDescriptorManager().getIndexedDescriptorSet();
    
    if (entityDescriptorSet != VK_NULL_HANDLE) {
        vk.vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1, &entityDescriptorSet,
            0, nullptr
        );
    } else {
        std::cerr << "EntityGraphicsNode: ERROR - Missing graphics descriptor set!" << std::endl;
        return;
    }

    // Push constants for vertex shader
    struct VertexPushConstants {
        float time;                 // Current simulation time
        float dt;                   // Time per frame  
        uint32_t count;             // Total number of entities
    } vertexPushConstants = { 
        frameTime,
        frameDeltaTime,
        entityCount
    };
    
    vk.vkCmdPushConstants(
        commandBuffer, 
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 
        0, sizeof(VertexPushConstants), 
        &vertexPushConstants
    );

    // Draw entities
    if (entityCount > 0) {
        // Bind vertex buffer: only geometry vertices (SoA uses storage buffers for entity data)
        VkBuffer vertexBuffers[] = {
            resourceCoordinator->getGraphicsManager()->getVertexBuffer()      // Vertex positions for triangle geometry
        };
        VkDeviceSize offsets[] = {0};
        vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Bind index buffer for triangle geometry
        vk.vkCmdBindIndexBuffer(
            commandBuffer, resourceCoordinator->getGraphicsManager()->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        
        // Draw indexed instances: all entities with triangle geometry
        vk.vkCmdDrawIndexed(
            commandBuffer, 
            resourceCoordinator->getGraphicsManager()->getIndexCount(),  // Number of indices per triangle
            entityCount,                      // Number of instances (entities)
            0, 0, 0                          // Index/vertex/instance offsets
        );
        
        // Debug: confirm draw call (thread-safe)
        FRAME_GRAPH_DEBUG_LOG_THROTTLED(drawCounter, 1800, "EntityGraphicsNode: Drew " << entityCount << " entities with " << resourceCoordinator->getGraphicsManager()->getIndexCount() << " indices per triangle");
    }

    // End dynamic rendering
    vk.vkCmdEndRendering(commandBuffer);
}

void EntityGraphicsNode::updateUniformBuffer() {
    if (!resourceCoordinator) return;
    
    // Check if uniform buffer needs updating for this frame index
    bool needsUpdate = uniformBufferDirty || (lastUpdatedFrameIndex != currentFrameIndex);
    
    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
    } newUBO{};

    // Get camera matrices from service
    auto& cameraService = ServiceLocator::instance().requireService<CameraService>();
    newUBO.view = cameraService.getViewMatrix();
    newUBO.proj = cameraService.getProjectionMatrix();
    
    // Debug camera matrix application (once every 30 seconds) - thread-safe
    if constexpr (FRAME_GRAPH_DEBUG_ENABLED) {
        uint32_t counter = FrameGraphDebug::incrementCounter(debugCounter);
        if (counter % 1800 == 0) {
            std::cout << "[FrameGraph Debug] EntityGraphicsNode: Using camera matrices from service (occurrence #" << counter << ")" << std::endl;
            std::cout << "  View matrix[3]: " << newUBO.view[3][0] << ", " << newUBO.view[3][1] << ", " << newUBO.view[3][2] << std::endl;
            std::cout << "  Proj matrix[0][0]: " << newUBO.proj[0][0] << ", [1][1]: " << newUBO.proj[1][1] << std::endl;
        }
    }
    
    // If no valid matrices, use fallback
    if (newUBO.view == glm::mat4(0.0f) || newUBO.proj == glm::mat4(0.0f)) {
        // Original fallback matrices when no world is set
        newUBO.view = glm::mat4(1.0f);
        newUBO.proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -5.0f, 5.0f);
        newUBO.proj[1][1] *= -1; // Flip Y for Vulkan
        
        FRAME_GRAPH_DEBUG_LOG_THROTTLED(debugCounter, 1800, "EntityGraphicsNode: Using fallback matrices - no world reference");
    }
    
    // Check if matrices actually changed (avoid memcmp by comparing key components)
    bool matricesChanged = (newUBO.view != cachedUBO.view) || (newUBO.proj != cachedUBO.proj);
    
    // Only update if dirty, frame changed, or matrices changed
    if (needsUpdate || matricesChanged) {
        auto uniformBuffers = resourceCoordinator->getGraphicsManager()->getUniformBuffersMapped();
        
        // Auto-recreate uniform buffers if they were destroyed (e.g., during resize)
        if (uniformBuffers.empty()) {
            std::cout << "EntityGraphicsNode: Uniform buffers missing, attempting to recreate..." << std::endl;
            if (resourceCoordinator->getGraphicsManager()->createAllGraphicsResources()) {
                std::cout << "EntityGraphicsNode: Successfully recreated graphics resources" << std::endl;
                uniformBuffers = resourceCoordinator->getGraphicsManager()->getUniformBuffersMapped();
            } else {
                std::cerr << "EntityGraphicsNode: CRITICAL ERROR: Failed to recreate graphics resources!" << std::endl;
                return;
            }
        }
        
        if (!uniformBuffers.empty() && currentFrameIndex < uniformBuffers.size()) {
            void* data = uniformBuffers[currentFrameIndex];
            if (data) {
                memcpy(data, &newUBO, sizeof(newUBO));
                
                // Update cache and tracking
                cachedUBO.view = newUBO.view;
                cachedUBO.proj = newUBO.proj;
                uniformBufferDirty = false;
                lastUpdatedFrameIndex = currentFrameIndex;
                
                // Debug optimized updates (once every 30 seconds) - thread-safe
                FRAME_GRAPH_DEBUG_LOG_THROTTLED(updateCounter, 1800, "EntityGraphicsNode: Updated uniform buffer (optimized)");
            }
        } else {
            std::cerr << "EntityGraphicsNode: ERROR: invalid currentFrameIndex (" << currentFrameIndex 
                     << ") or uniformBuffers size (" << uniformBuffers.size() << ")!" << std::endl;
        }
    }
}

// Optional dependency validation
void EntityGraphicsNode::onFirstUse(const FrameGraph& frameGraph) {
    // Dependencies validated in constructor
}
