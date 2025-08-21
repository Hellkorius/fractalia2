#include "entity_graphics_node.h"
#include "../pipelines/graphics_pipeline_manager.h"
#include "../core/vulkan_swapchain.h"
#include "../resources/resource_context.h"
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
    ResourceContext* resourceContext,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , colorTargetId(colorTarget)
  , graphicsManager(graphicsManager)
  , swapchain(swapchain)
  , resourceContext(resourceContext)
  , gpuEntityManager(gpuEntityManager) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!graphicsManager) {
        throw std::invalid_argument("EntityGraphicsNode: graphicsManager cannot be null");
    }
    if (!swapchain) {
        throw std::invalid_argument("EntityGraphicsNode: swapchain cannot be null");
    }
    if (!resourceContext) {
        throw std::invalid_argument("EntityGraphicsNode: resourceContext cannot be null");
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

void EntityGraphicsNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    
    // Validate dependencies are still valid
    if (!graphicsManager || !swapchain || !resourceContext || !gpuEntityManager) {
        std::cerr << "EntityGraphicsNode: Critical error - dependencies became null during execution" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    
    if (entityCount == 0) {
        uint32_t counter = noEntitiesCounter.fetch_add(1, std::memory_order_relaxed);
        if (counter % 60 == 0) {
            std::cout << "EntityGraphicsNode: No entities to render" << std::endl;
        }
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
    
    // Create graphics pipeline state for entity rendering - use same layout as ResourceContext
    auto layoutSpec = DescriptorLayoutPresets::createEntityGraphicsLayout();
    VkDescriptorSetLayout descriptorLayout = graphicsManager->getLayoutManager()->getLayout(layoutSpec);
    
    // Get the render pass that was used to create the framebuffers
    VkRenderPass renderPass = graphicsManager->createRenderPass(
        swapchain->getImageFormat(), 
        VK_FORMAT_UNDEFINED,  // No depth (match VulkanRenderer setup)
        VK_SAMPLE_COUNT_2_BIT, // MSAA samples
        true  // Enable MSAA
    );
    
    GraphicsPipelineState pipelineState = GraphicsPipelinePresets::createEntityRenderingState(
        renderPass, descriptorLayout);
    
    // Get pipeline and layout
    VkPipeline pipeline = graphicsManager->getPipeline(pipelineState);
    
    VkPipelineLayout pipelineLayout = graphicsManager->getPipelineLayout(pipelineState);
    
    if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
        std::cerr << "EntityGraphicsNode: Failed to get graphics pipeline" << std::endl;
        return;
    }
    
    // Validate swapchain state before accessing framebuffers
    const auto& framebuffers = swapchain->getFramebuffers();
    if (imageIndex >= framebuffers.size()) {
        std::cerr << "EntityGraphicsNode: Invalid imageIndex " << imageIndex 
                  << " >= framebuffer count " << framebuffers.size() << std::endl;
        return;
    }
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->getExtent();

    // Clear values: MSAA color, resolve color (no depth)
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.2f, 1.0f}};  // MSAA color attachment
    clearValues[1].color = {{0.1f, 0.1f, 0.2f, 1.0f}};  // Resolve attachment
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Cache loader reference for performance
    const auto& vk = context->getLoader();

    vk.vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    
    // Bind single descriptor set with unified layout (uniform + storage buffers)
    VkDescriptorSet entityDescriptorSet = gpuEntityManager->getDescriptorManager().getGraphicsDescriptorSet();
    
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
            resourceContext->getVertexBuffer()      // Vertex positions for triangle geometry
        };
        VkDeviceSize offsets[] = {0};
        vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Bind index buffer for triangle geometry
        vk.vkCmdBindIndexBuffer(
            commandBuffer, resourceContext->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        
        // Draw indexed instances: all entities with triangle geometry
        vk.vkCmdDrawIndexed(
            commandBuffer, 
            resourceContext->getIndexCount(),  // Number of indices per triangle
            entityCount,                      // Number of instances (entities)
            0, 0, 0                          // Index/vertex/instance offsets
        );
        
        // Debug: confirm draw call (thread-safe)
        uint32_t counter = drawCounter.fetch_add(1, std::memory_order_relaxed);
        if (counter % 60 == 0) {
            std::cout << "EntityGraphicsNode: Drew " << entityCount 
                      << " entities with " << resourceContext->getIndexCount() 
                      << " indices per triangle" << std::endl;
        }
    }

    // End render pass
    vk.vkCmdEndRenderPass(commandBuffer);
}

void EntityGraphicsNode::updateUniformBuffer() {
    if (!resourceContext) return;
    
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
    
    // Debug camera matrix application (once per second) - thread-safe
    uint32_t counter = debugCounter.fetch_add(1, std::memory_order_relaxed);
    if (counter % 60 == 0) {
        std::cout << "EntityGraphicsNode: Using camera matrices from service" << std::endl;
        std::cout << "  View matrix[3]: " << newUBO.view[3][0] << ", " << newUBO.view[3][1] << ", " << newUBO.view[3][2] << std::endl;
        std::cout << "  Proj matrix[0][0]: " << newUBO.proj[0][0] << ", [1][1]: " << newUBO.proj[1][1] << std::endl;
    }
    
    // If no valid matrices, use fallback
    if (newUBO.view == glm::mat4(0.0f) || newUBO.proj == glm::mat4(0.0f)) {
        // Original fallback matrices when no world is set
        newUBO.view = glm::mat4(1.0f);
        newUBO.proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -5.0f, 5.0f);
        newUBO.proj[1][1] *= -1; // Flip Y for Vulkan
        
        uint32_t counter = debugCounter.fetch_add(1, std::memory_order_relaxed);
        if (counter % 60 == 0) {
            std::cout << "EntityGraphicsNode: Using fallback matrices - no world reference" << std::endl;
        }
    }
    
    // Check if matrices actually changed (avoid memcmp by comparing key components)
    bool matricesChanged = (newUBO.view != cachedUBO.view) || (newUBO.proj != cachedUBO.proj);
    
    // Only update if dirty, frame changed, or matrices changed
    if (needsUpdate || matricesChanged) {
        auto uniformBuffers = resourceContext->getUniformBuffersMapped();
        
        // Auto-recreate uniform buffers if they were destroyed (e.g., during resize)
        if (uniformBuffers.empty()) {
            std::cout << "EntityGraphicsNode: Uniform buffers missing, attempting to recreate..." << std::endl;
            if (resourceContext->createGraphicsResources()) {
                std::cout << "EntityGraphicsNode: Successfully recreated graphics resources" << std::endl;
                uniformBuffers = resourceContext->getUniformBuffersMapped();
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
                
                // Debug optimized updates (once per second) - thread-safe
                uint32_t counter = updateCounter.fetch_add(1, std::memory_order_relaxed);
                if (counter % 60 == 0) {
                    std::cout << "EntityGraphicsNode: Updated uniform buffer (optimized)" << std::endl;
                }
            }
        } else {
            std::cerr << "EntityGraphicsNode: ERROR: invalid currentFrameIndex (" << currentFrameIndex 
                     << ") or uniformBuffers size (" << uniformBuffers.size() << ")!" << std::endl;
        }
    }
}

// Node lifecycle implementation
bool EntityGraphicsNode::initializeNode(const FrameGraph& frameGraph) {
    // One-time initialization - validate dependencies
    if (!graphicsManager) {
        std::cerr << "EntityGraphicsNode: GraphicsPipelineManager is null" << std::endl;
        return false;
    }
    if (!swapchain) {
        std::cerr << "EntityGraphicsNode: VulkanSwapchain is null" << std::endl;
        return false;
    }
    if (!resourceContext) {
        std::cerr << "EntityGraphicsNode: ResourceContext is null" << std::endl;
        return false;
    }
    if (!gpuEntityManager) {
        std::cerr << "EntityGraphicsNode: GPUEntityManager is null" << std::endl;
        return false;
    }
    return true;
}

void EntityGraphicsNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    // Store timing data
    frameTime = time;
    frameDeltaTime = deltaTime;
    currentFrameIndex = frameIndex;
    
    // Check if uniform buffer needs updating
    if (uniformBufferDirty || lastUpdatedFrameIndex != frameIndex) {
        updateUniformBuffer();
    }
}

void EntityGraphicsNode::releaseFrame(uint32_t frameIndex) {
    // Per-frame cleanup - nothing to clean up for graphics node
}