#include "entity_graphics_node.h"
#include "../vulkan_pipeline.h"
#include "../vulkan_swapchain.h"
#include "../resource_context.h"
#include "../../ecs/gpu_entity_manager.h"
#include "../vulkan_context.h"
#include "../vulkan_function_loader.h"
#include <iostream>
#include <array>

EntityGraphicsNode::EntityGraphicsNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId colorTarget,
    VulkanPipeline* pipeline,
    VulkanSwapchain* swapchain,
    ResourceContext* resourceContext,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , colorTargetId(colorTarget)
  , pipeline(pipeline)
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
    if (!pipeline || !swapchain || !resourceContext || !gpuEntityManager) {
        std::cerr << "EntityGraphicsNode: Missing dependencies" << std::endl;
        return;
    }
    
    // Get Vulkan context from frame graph
    const VulkanContext* context = frameGraph.getContext();
    if (!context) {
        std::cerr << "EntityGraphicsNode: Missing Vulkan context" << std::endl;
        return;
    }
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = pipeline->getRenderPass();
    renderPassInfo.framebuffer = swapchain->getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->getExtent();

    // Clear values: MSAA color, resolve color, depth
    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.2f, 1.0f}};  // MSAA color attachment
    clearValues[1].color = {{0.1f, 0.1f, 0.2f, 1.0f}};  // Resolve attachment
    clearValues[2].depthStencil = {1.0f, 0};             // Depth attachment
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    context->getLoader().vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Get entity count for rendering
    uint32_t gpuEntityCount = gpuEntityManager->getEntityCount();
    
    // Bind graphics pipeline
    context->getLoader().vkCmdBindPipeline(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipeline->getGraphicsPipeline()
    );
    
    // Bind descriptor sets (uniform buffers, etc.)
    const auto& descriptorSets = resourceContext->getGraphicsDescriptorSets();
    // Note: Using currentFrameIndex for descriptor set selection
    context->getLoader().vkCmdBindDescriptorSets(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipeline->getPipelineLayout(), 
        0, 1, &descriptorSets[currentFrameIndex], 0, nullptr
    );

    // Push constants for vertex shader
    struct VertexPushConstants {
        float time;                 // Current simulation time
        float dt;                   // Time per frame  
        uint32_t count;             // Total number of entities
    } vertexPushConstants = { 
        frameTime,
        frameDeltaTime,
        gpuEntityCount
    };
    
    context->getLoader().vkCmdPushConstants(
        commandBuffer, 
        pipeline->getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 
        0, sizeof(VertexPushConstants), 
        &vertexPushConstants
    );

    // Set viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain->getExtent().width);
    viewport.height = static_cast<float>(swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    context->getLoader().vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // Set scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    context->getLoader().vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Draw entities if we have any
    if (gpuEntityCount > 0) {
        // Bind vertex buffers: geometry vertices + entity instance data
        VkBuffer vertexBuffers[] = {
            resourceContext->getVertexBuffer(),           // Vertex positions for triangle geometry
            gpuEntityManager->getCurrentEntityBuffer()    // Per-instance entity data
        };
        VkDeviceSize offsets[] = {0, 0};
        context->getLoader().vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
        
        // Bind index buffer for triangle geometry
        context->getLoader().vkCmdBindIndexBuffer(
            commandBuffer, 
            resourceContext->getIndexBuffer(), 
            0, 
            VK_INDEX_TYPE_UINT16
        );
        
        // Draw indexed instances: all entities with triangle geometry
        context->getLoader().vkCmdDrawIndexed(
            commandBuffer, 
            resourceContext->getIndexCount(),  // Number of indices per triangle
            gpuEntityCount,                   // Number of instances (entities)
            0, 0, 0                          // Index/vertex/instance offsets
        );
        
        std::cout << "EntityGraphicsNode: Drew " << gpuEntityCount 
                  << " entities with " << resourceContext->getIndexCount() << " indices each" << std::endl;
    }

    // End render pass
    context->getLoader().vkCmdEndRenderPass(commandBuffer);
}