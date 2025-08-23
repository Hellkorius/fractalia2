#include "shadow_pass_node.h"
#include "../pipelines/graphics_pipeline_manager.h"
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

ShadowPassNode::ShadowPassNode(
    FrameGraphTypes::ResourceId entityBuffer, 
    FrameGraphTypes::ResourceId positionBuffer,
    FrameGraphTypes::ResourceId shadowDepthTarget,
    GraphicsPipelineManager* graphicsManager,
    ResourceCoordinator* resourceCoordinator,
    GPUEntityManager* gpuEntityManager
) : entityBufferId(entityBuffer)
  , positionBufferId(positionBuffer)
  , shadowDepthTargetId(shadowDepthTarget)
  , graphicsManager(graphicsManager)
  , resourceCoordinator(resourceCoordinator)
  , gpuEntityManager(gpuEntityManager) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!graphicsManager) {
        throw std::invalid_argument("ShadowPassNode: graphicsManager cannot be null");
    }
    if (!resourceCoordinator) {
        throw std::invalid_argument("ShadowPassNode: resourceCoordinator cannot be null");
    }
    if (!gpuEntityManager) {
        throw std::invalid_argument("ShadowPassNode: gpuEntityManager cannot be null");
    }
    
    // Initialize cascade splits for 3 cascades (near, middle, far)
    cascadeSplits = {0.1f, 0.3f, 1.0f};
    
    // Initialize shadow UBO with sensible defaults
    shadowUBO.sunDirection = glm::vec4(sunDirection, 0.0f);
    shadowUBO.shadowDistance = shadowDistance;
    shadowUBO.cascadeCount = cascadeCount;
    shadowUBO.bias = 0.005f;
    shadowUBO.normalOffset = 0.1f;
}

std::vector<ResourceDependency> ShadowPassNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
        {positionBufferId, ResourceAccess::Read, PipelineStage::VertexShader}
    };
}

std::vector<ResourceDependency> ShadowPassNode::getOutputs() const {
    return {
        {shadowDepthTargetId, ResourceAccess::Write, PipelineStage::DepthAttachment}
    };
}

bool ShadowPassNode::initializeNode(const FrameGraph& frameGraph) {
    try {
        // Basic validation - resources will be validated during execution
        // Frame graph doesn't expose hasResource method
        
        std::cout << "ShadowPassNode initialized successfully with " << cascadeCount << " cascades" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ShadowPassNode::initializeNode exception: " << e.what() << std::endl;
        return false;
    }
}

void ShadowPassNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    frameTime = time;
    frameDeltaTime = deltaTime;
    currentFrameIndex = frameIndex;
    
    // Calculate cascaded shadow matrices if data is dirty or frame changed
    if (shadowDataDirty || lastUpdatedFrameIndex != frameIndex) {
        calculateCascadedShadowMatrices();
        updateShadowUniformBuffer();
        shadowDataDirty = false;
        lastUpdatedFrameIndex = frameIndex;
    }
}

void ShadowPassNode::releaseFrame(uint32_t frameIndex) {
    // No per-frame cleanup needed for shadow pass
}

void ShadowPassNode::calculateCascadedShadowMatrices() {
    if (!world) {
        std::cerr << "ShadowPassNode::calculateCascadedShadowMatrices: world is null" << std::endl;
        return;
    }
    
    try {
        // Get camera service for view frustum information
        CameraService& cameraService = SERVICE(CameraService);
        
        // Get camera matrices
        glm::mat4 viewMatrix = cameraService.getViewMatrix();
        glm::mat4 projMatrix = cameraService.getProjectionMatrix();
        glm::mat4 invViewProj = glm::inverse(projMatrix * viewMatrix);
        
        // Calculate frustum corners in world space for each cascade
        for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade) {
            float nearPlane = cascade == 0 ? 0.1f : cascadeSplits[cascade - 1] * shadowDistance;
            float farPlane = cascadeSplits[cascade] * shadowDistance;
            
            // Calculate frustum corners in NDC space
            std::array<glm::vec4, 8> frustumCorners = {{
                // Near face corners
                {-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f, 1.0f},
                {1.0f, 1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f, 1.0f},
                // Far face corners  
                {-1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}
            }};
            
            // Transform to world space and adjust Z values for cascade
            glm::vec3 center(0.0f);
            for (int i = 0; i < 8; ++i) {
                if (i < 4) {
                    // Near face - adjust Z to nearPlane
                    frustumCorners[i].z = (nearPlane - 0.1f) / (shadowDistance - 0.1f) * 2.0f - 1.0f;
                } else {
                    // Far face - adjust Z to farPlane
                    frustumCorners[i].z = (farPlane - 0.1f) / (shadowDistance - 0.1f) * 2.0f - 1.0f;
                }
                
                glm::vec4 worldCorner = invViewProj * frustumCorners[i];
                worldCorner /= worldCorner.w;
                frustumCorners[i] = worldCorner;
                center += glm::vec3(worldCorner) / 8.0f;
            }
            
            // Calculate bounding sphere radius for stable shadow maps
            float radius = 0.0f;
            for (const auto& corner : frustumCorners) {
                float distance = glm::length(glm::vec3(corner) - center);
                radius = glm::max(radius, distance);
            }
            
            // Snap center to texel grid for stable shadows
            const float texelSize = (radius * 2.0f) / 2048.0f; // Assuming 2048x2048 shadow map
            center.x = glm::floor(center.x / texelSize) * texelSize;
            center.y = glm::floor(center.y / texelSize) * texelSize;
            center.z = glm::floor(center.z / texelSize) * texelSize;
            
            // Create light view matrix looking towards the frustum center
            glm::vec3 lightPos = center - sunDirection * (radius + 50.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
            
            // Create orthographic projection for directional light
            glm::mat4 lightProj = glm::ortho(
                -radius, radius,
                -radius, radius, 
                0.1f, radius * 2.0f + 100.0f
            );
            
            shadowUBO.lightSpaceMatrices[cascade] = lightProj * lightView;
        }
        
        // Update cascade splits in UBO
        shadowUBO.cascadeSplits = glm::vec4(
            cascadeSplits[0], 
            cascadeCount > 1 ? cascadeSplits[1] : 1.0f,
            cascadeCount > 2 ? cascadeSplits[2] : 1.0f,
            cascadeCount > 3 ? cascadeSplits[3] : 1.0f
        );
        
    } catch (const std::exception& e) {
        std::cerr << "ShadowPassNode::calculateCascadedShadowMatrices exception: " << e.what() << std::endl;
    }
}

void ShadowPassNode::updateShadowUniformBuffer() {
    // Update sun direction and other parameters
    shadowUBO.sunDirection = glm::vec4(sunDirection, 0.0f);
    shadowUBO.shadowDistance = shadowDistance;
    shadowUBO.cascadeCount = cascadeCount;
    
    // TODO: Update actual GPU uniform buffer through resource coordinator
    // This will be implemented when the uniform buffer resource is set up
}

void ShadowPassNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        debugCounter++;
        
        // Get entity count from GPU entity manager
        uint32_t entityCount = gpuEntityManager->getEntityCount();
        if (entityCount == 0) {
            return; // Nothing to render
        }
        
        // Get Vulkan context and function loader from frame graph
        const VulkanContext* context = frameGraph.getContext();
        if (!context) {
            std::cerr << "ShadowPassNode::execute: VulkanContext is null" << std::endl;
            return;
        }
        
        const auto& vk = context->getLoader();
        
        // Begin shadow pass render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = VK_NULL_HANDLE; // TODO: Get shadow render pass from graphics manager
        renderPassInfo.framebuffer = VK_NULL_HANDLE; // TODO: Get shadow framebuffer from resource
        
        VkRect2D renderArea{};
        renderArea.offset = {0, 0};
        renderArea.extent = {2048, 2048}; // Shadow map resolution
        renderPassInfo.renderArea = renderArea;
        
        VkClearValue clearValue{};
        clearValue.depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;
        
        // TODO: Begin render pass when shadow render pass is set up
        // vk.vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // Set viewport for shadow map
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 2048.0f;
        viewport.height = 2048.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vk.vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {2048, 2048};
        vk.vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        // Render each cascade
        for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade) {
            // TODO: Bind shadow pipeline and descriptor sets
            // TODO: Update push constants with cascade index
            // TODO: Draw instanced entities for this cascade
            
            renderCounter++;
        }
        
        // TODO: End render pass when shadow render pass is set up
        // vk.vkCmdEndRenderPass(commandBuffer);
        
    } catch (const std::exception& e) {
        std::cerr << "ShadowPassNode::execute exception: " << e.what() << std::endl;
    }
}