#include "particle_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../resources/core/resource_coordinator.h"
#include "../../ecs/core/service_locator.h"
#include "../../ecs/services/camera_service.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include <iostream>
#include <array>
#include <flecs.h>
#include <stdexcept>

ParticleComputeNode::ParticleComputeNode(
    FrameGraphTypes::ResourceId particleBuffer,
    ComputePipelineManager* computeManager,
    ResourceCoordinator* resourceCoordinator
) : particleBufferId(particleBuffer)
  , computeManager(computeManager)
  , resourceCoordinator(resourceCoordinator) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!computeManager) {
        throw std::invalid_argument("ParticleComputeNode: computeManager cannot be null");
    }
    if (!resourceCoordinator) {
        throw std::invalid_argument("ParticleComputeNode: resourceCoordinator cannot be null");
    }
    
    // Initialize particle UBO with defaults
    particleUBO.sunDirection = glm::vec4(sunDirection, 1.0f);
    particleUBO.sceneCenter = sceneCenter;
    particleUBO.maxParticles = maxParticles;
    particleUBO.emissionRate = emissionRate;
    particleUBO.particleLifetime = particleLifetime;
    particleUBO.windStrength = windStrength;
    particleUBO.gravityStrength = gravityStrength;
    particleUBO.sunRayLength = sunRayLength;
}

std::vector<ResourceDependency> ParticleComputeNode::getInputs() const {
    return {
        {particleBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader}
    };
}

std::vector<ResourceDependency> ParticleComputeNode::getOutputs() const {
    return {
        {particleBufferId, ResourceAccess::Write, PipelineStage::ComputeShader}
    };
}

bool ParticleComputeNode::initializeNode(const FrameGraph& frameGraph) {
    try {
        std::cout << "ParticleComputeNode initialized for " << maxParticles << " particles" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ParticleComputeNode::initializeNode exception: " << e.what() << std::endl;
        return false;
    }
}

void ParticleComputeNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    frameTime = time;
    frameDeltaTime = deltaTime;
    currentFrameIndex = frameIndex;
    
    // Update particle UBO data
    updateParticleUBO();
}

void ParticleComputeNode::releaseFrame(uint32_t frameIndex) {
    // No per-frame cleanup needed for particle compute
}

void ParticleComputeNode::updateParticleUBO() {
    if (!world) {
        // Use fallback matrices when no world reference
        particleUBO.viewMatrix = glm::mat4(1.0f);
        particleUBO.projMatrix = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
    } else {
        try {
            // Get camera matrices from service
            CameraService& cameraService = SERVICE(CameraService);
            particleUBO.viewMatrix = cameraService.getViewMatrix();
            particleUBO.projMatrix = cameraService.getProjectionMatrix();
        } catch (const std::exception& e) {
            // Fallback if service not available
            particleUBO.viewMatrix = glm::mat4(1.0f);
            particleUBO.projMatrix = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
        }
    }
    
    // Update timing and configuration
    particleUBO.deltaTime = frameDeltaTime;
    particleUBO.totalTime = frameTime;
    particleUBO.sunDirection = glm::vec4(sunDirection, 1.0f);
    particleUBO.sceneCenter = sceneCenter;
    particleUBO.maxParticles = maxParticles;
    particleUBO.emissionRate = emissionRate;
    particleUBO.particleLifetime = particleLifetime;
    particleUBO.windStrength = windStrength;
    particleUBO.gravityStrength = gravityStrength;
    particleUBO.sunRayLength = sunRayLength;
    
    // Calculate virtual sun position for shader
    particleUBO.sunPosition = glm::vec4(glm::vec3(sceneCenter) - sunDirection * sunRayLength * 0.5f, 1.0f);
    
    uboNeedsUpdate = true;
}

void ParticleComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        debugCounter++;
        
        // Get Vulkan context and function loader from frame graph
        const VulkanContext* context = frameGraph.getContext();
        if (!context) {
            std::cerr << "ParticleComputeNode::execute: VulkanContext is null" << std::endl;
            return;
        }
        
        const auto& vk = context->getLoader();
        
        // Create particle compute pipeline state
        auto layoutSpec = DescriptorLayoutPresets::createParticleComputeLayout();
        VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
        ComputePipelineState pipelineState = ComputePipelinePresets::createParticleUpdateState(descriptorLayout);
        
        // Create compute dispatch
        ComputeDispatch dispatch{};
        dispatch.pipeline = computeManager->getPipeline(pipelineState);
        dispatch.layout = computeManager->getPipelineLayout(pipelineState);
        
        if (dispatch.pipeline == VK_NULL_HANDLE || dispatch.layout == VK_NULL_HANDLE) {
            std::cerr << "ParticleComputeNode: Failed to get particle compute pipeline or layout" << std::endl;
            return;
        }
        
        // Set up push constants
        struct ParticlePushConstants {
            float time;
            float deltaTime;
        } pushConstants;
        
        pushConstants.time = frameTime;
        pushConstants.deltaTime = frameDeltaTime;
        
        dispatch.pushConstantData = &pushConstants;
        dispatch.pushConstantSize = sizeof(ParticlePushConstants);
        dispatch.pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // Calculate dispatch parameters
        uint32_t workgroupSize = 64; // From shader local_size_x
        uint32_t numWorkgroups = (maxParticles + workgroupSize - 1) / workgroupSize;
        dispatch.calculateOptimalDispatch(maxParticles, glm::uvec3(workgroupSize, 1, 1));
        
        // TODO: Set up descriptor sets (particle buffer and UBO)
        // For now dispatch without descriptor sets - will need to add particle buffer binding
        
        // Execute the compute dispatch
        computeManager->dispatch(commandBuffer, dispatch);
        
        computeCounter++;
        
        uint32_t counter = debugCounter.load();
        if (counter % 1800 == 0) { // Every 30 seconds at 60 FPS
            std::cout << "ParticleComputeNode: Would dispatch " << numWorkgroups << " workgroups for " << maxParticles << " particles" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ParticleComputeNode::execute exception: " << e.what() << std::endl;
    }
}