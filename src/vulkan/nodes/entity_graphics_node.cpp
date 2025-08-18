#include "entity_graphics_node.h"
#include "../pipelines/graphics_pipeline_manager.h"
#include "../core/vulkan_swapchain.h"
#include "../resources/resource_context.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../../ecs/systems/camera_system.h"
#include "../../ecs/camera_component.h"
#include "../pipelines/descriptor_layout_manager.h"
#include <iostream>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>

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
}

std::vector<ResourceDependency> EntityGraphicsNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
        {positionBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
    };
}

std::vector<ResourceDependency> EntityGraphicsNode::getOutputs() const {
    return {
        {colorTargetId, ResourceAccess::Write, PipelineStage::ColorAttachment},
    };
}

void EntityGraphicsNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    if (!graphicsManager || !swapchain || !resourceContext || !gpuEntityManager) {
        std::cerr << "EntityGraphicsNode: Missing dependencies" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    if (entityCount == 0) {
        static int noEntitiesCounter = 0;
        if (noEntitiesCounter++ % 60 == 0) {
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
    
    // Update uniform buffer with camera matrices
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
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchain->getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->getExtent();

    // Clear values: MSAA color, resolve color (no depth)
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.2f, 1.0f}};  // MSAA color attachment
    clearValues[1].color = {{0.1f, 0.1f, 0.2f, 1.0f}};  // Resolve attachment
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    context->getLoader().vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain->getExtent().width);
    viewport.height = static_cast<float>(swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    context->getLoader().vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    context->getLoader().vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Entity count already retrieved above
    
    // Bind graphics pipeline
    context->getLoader().vkCmdBindPipeline(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipeline
    );
    
    // Bind descriptor sets from ResourceContext (includes uniform buffer + position buffer)
    const auto& resourceDescriptorSets = resourceContext->getGraphicsDescriptorSets();
    if (!resourceDescriptorSets.empty() && currentFrameIndex < resourceDescriptorSets.size()) {
        VkDescriptorSet descriptorSet = resourceDescriptorSets[currentFrameIndex];
        context->getLoader().vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1, &descriptorSet,
            0, nullptr
        );
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
    
    context->getLoader().vkCmdPushConstants(
        commandBuffer, 
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 
        0, sizeof(VertexPushConstants), 
        &vertexPushConstants
    );

    // Draw entities
    if (entityCount > 0) {
        // Bind vertex buffers: geometry vertices + entity instance data
        VkBuffer vertexBuffers[] = {
            resourceContext->getVertexBuffer(),     // Vertex positions for triangle geometry
            gpuEntityManager->getEntityBuffer()     // Per-instance entity data
        };
        VkDeviceSize offsets[] = {0, 0};
        context->getLoader().vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
        
        // Bind index buffer for triangle geometry
        context->getLoader().vkCmdBindIndexBuffer(
            commandBuffer, resourceContext->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        
        // Draw indexed instances: all entities with triangle geometry
        context->getLoader().vkCmdDrawIndexed(
            commandBuffer, 
            resourceContext->getIndexCount(),  // Number of indices per triangle
            entityCount,                      // Number of instances (entities)
            0, 0, 0                          // Index/vertex/instance offsets
        );
        
        // Debug: confirm draw call
        static int drawCounter = 0;
        if (drawCounter++ % 60 == 0) {
            std::cout << "EntityGraphicsNode: Drew " << entityCount 
                      << " entities with " << resourceContext->getIndexCount() 
                      << " indices per triangle" << std::endl;
        }
    }

    // End render pass
    context->getLoader().vkCmdEndRenderPass(commandBuffer);
}

void EntityGraphicsNode::updateUniformBuffer() {
    if (!resourceContext) return;
    
    // Check if uniform buffer needs updating for this frame index
    bool needsUpdate = uniformBufferDirty || (lastUpdatedFrameIndex != currentFrameIndex);
    
    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
    } newUBO{};

    // Get camera matrices from ECS if available
    if (world != nullptr) {
        auto cameraMatrices = CameraManager::getCameraMatrices(*world);
        if (cameraMatrices.valid) {
            newUBO.view = cameraMatrices.view;
            newUBO.proj = cameraMatrices.projection;
            
            // Debug camera matrix application (once per second)
            static int debugCounter = 0;
            if (debugCounter++ % 60 == 0) {
                std::cout << "EntityGraphicsNode: Using camera matrices from ECS" << std::endl;
                std::cout << "  View matrix[3]: " << newUBO.view[3][0] << ", " << newUBO.view[3][1] << ", " << newUBO.view[3][2] << std::endl;
                std::cout << "  Proj matrix[0][0]: " << newUBO.proj[0][0] << ", [1][1]: " << newUBO.proj[1][1] << std::endl;
            }
        } else {
            // Fallback to default matrices if no camera found
            newUBO.view = glm::mat4(1.0f);
            newUBO.proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -5.0f, 5.0f);
            newUBO.proj[1][1] *= -1; // Flip Y for Vulkan
            
            static int fallbackCounter = 0;
            if (fallbackCounter++ % 60 == 0) {
                std::cout << "EntityGraphicsNode: Using fallback matrices - no valid camera" << std::endl;
            }
        }
    } else {
        // Original fallback matrices when no world is set
        newUBO.view = glm::mat4(1.0f);
        newUBO.proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -5.0f, 5.0f);
        newUBO.proj[1][1] *= -1; // Flip Y for Vulkan
        
        static int noWorldCounter = 0;
        if (noWorldCounter++ % 60 == 0) {
            std::cout << "EntityGraphicsNode: Using fallback matrices - no world reference" << std::endl;
        }
    }
    
    // Check if matrices actually changed (avoid memcmp by comparing key components)
    bool matricesChanged = (newUBO.view != cachedUBO.view) || (newUBO.proj != cachedUBO.proj);
    
    // Only update if dirty, frame changed, or matrices changed
    if (needsUpdate || matricesChanged) {
        auto uniformBuffers = resourceContext->getUniformBuffersMapped();
        if (!uniformBuffers.empty() && currentFrameIndex < uniformBuffers.size()) {
            void* data = uniformBuffers[currentFrameIndex];
            if (data) {
                memcpy(data, &newUBO, sizeof(newUBO));
                
                // Update cache and tracking
                cachedUBO.view = newUBO.view;
                cachedUBO.proj = newUBO.proj;
                uniformBufferDirty = false;
                lastUpdatedFrameIndex = currentFrameIndex;
                
                // Debug optimized updates (once per second)
                static int updateCounter = 0;
                if (updateCounter++ % 60 == 0) {
                    std::cout << "EntityGraphicsNode: Updated uniform buffer (optimized)" << std::endl;
                }
            }
        } else {
            std::cerr << "EntityGraphicsNode: ERROR: invalid currentFrameIndex or empty uniformBuffers!" << std::endl;
        }
    }
}