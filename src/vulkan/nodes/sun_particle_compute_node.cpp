#include "sun_particle_compute_node.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../../ecs/core/service_locator.h"
#include "../../ecs/services/camera_service.h"
#include <iostream>
#include <stdexcept>

SunParticleComputeNode::SunParticleComputeNode(
    ComputePipelineManager* computeManager
) : computeManager(computeManager) {
    
    if (!computeManager) {
        throw std::invalid_argument("SunParticleComputeNode: computeManager cannot be null");
    }
    
    std::cout << "SunParticleComputeNode: Created compute node for " << maxParticles << " particles" << std::endl;
}

SunParticleComputeNode::~SunParticleComputeNode() {
    if (descriptorPool != VK_NULL_HANDLE && vulkanContext) {
        const auto& vk = vulkanContext->getLoader();
        const VkDevice device = vulkanContext->getDevice();
        vk.vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

std::vector<ResourceDependency> SunParticleComputeNode::getInputs() const {
    std::vector<ResourceDependency> inputs;
    
    if (particleBufferId != 0) {
        inputs.push_back({particleBufferId, ResourceAccess::Read, PipelineStage::ComputeShader});
    }
    
    return inputs;
}

std::vector<ResourceDependency> SunParticleComputeNode::getOutputs() const {
    std::vector<ResourceDependency> outputs;
    
    if (particleBufferId != 0) {
        outputs.push_back({particleBufferId, ResourceAccess::Write, PipelineStage::ComputeShader});
    }
    
    return outputs;
}

bool SunParticleComputeNode::initializeNode(const FrameGraph& frameGraph) {
    try {
        vulkanContext = frameGraph.getContext();
        if (!vulkanContext) {
            std::cerr << "SunParticleComputeNode: VulkanContext is null" << std::endl;
            return false;
        }
        
        std::cout << "SunParticleComputeNode: Initialized compute node" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "SunParticleComputeNode::initializeNode exception: " << e.what() << std::endl;
        return false;
    }
}

void SunParticleComputeNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    frameTime = time;
    frameDeltaTime = deltaTime;
    currentFrameIndex = frameIndex;
    
    updateSunUBO();
}

void SunParticleComputeNode::releaseFrame(uint32_t frameIndex) {
    // No per-frame cleanup needed
}

void SunParticleComputeNode::updateSunUBO() {
    // Get camera matrices
    if (world) {
        try {
            CameraService& cameraService = SERVICE(CameraService);
            sunUBO.viewMatrix = cameraService.getViewMatrix();
            sunUBO.projMatrix = cameraService.getProjectionMatrix();
            sunUBO.cameraPos = glm::vec4(cameraService.getCameraPosition(), 90.0f);
        } catch (const std::exception& e) {
            // Fallback matrices
            sunUBO.viewMatrix = glm::mat4(1.0f);
            sunUBO.projMatrix = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
            sunUBO.cameraPos = glm::vec4(0.0f, 10.0f, 20.0f, 90.0f);
        }
    } else {
        sunUBO.viewMatrix = glm::mat4(1.0f);
        sunUBO.projMatrix = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
        sunUBO.cameraPos = glm::vec4(0.0f, 10.0f, 20.0f, 90.0f);
    }
    
    // Update sun parameters
    sunUBO.sunPosition = glm::vec4(sunPosition, sunRadius);
    sunUBO.sunColor = glm::vec4(sunColor, sunIntensity);
    sunUBO.sceneInfo = glm::vec4(frameTime, frameDeltaTime, float(maxParticles), windStrength);
    sunUBO.lightParams = glm::vec4(rayLength, rayIntensity, 1.0f, gravityStrength);
    
    uboNeedsUpdate = true;
}

void SunParticleComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        computeCounter++;
        
        // Particle buffer will be created during resource initialization if needed
        
        // Initialize resources on first execution (to avoid race conditions)
        if (!resourcesInitialized) {
            std::cout << "SunParticleComputeNode: Creating compute resources on first execution" << std::endl;
            
            // Create particle buffer in frame graph if not exists
            if (particleBufferId == 0) {
                VkDeviceSize particleBufferSize = maxParticles * sizeof(SunParticle);
                particleBufferId = const_cast<FrameGraph&>(frameGraph).createBuffer(
                    "SunParticleBuffer",
                    particleBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                );
                std::cout << "SunParticleComputeNode: Created particle buffer ID: " << particleBufferId 
                          << " (" << particleBufferSize << " bytes)" << std::endl;
            }
            
            // Create descriptor pool for compute
            const auto& vk = vulkanContext->getLoader();
            const VkDevice device = vulkanContext->getDevice();
            
            VkDescriptorPoolSize poolSizes[2];
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = 1;
            
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            poolSizes[1].descriptorCount = 1;
            
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 2;
            poolInfo.pPoolSizes = poolSizes;
            
            VkResult result = vk.vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
            if (result != VK_SUCCESS) {
                std::cerr << "SunParticleComputeNode: Failed to create descriptor pool" << std::endl;
                return;
            }
            
            resourcesInitialized = true;
            std::cout << "SunParticleComputeNode: Compute resources created successfully" << std::endl;
        }
        
        // Execute particle physics compute
        executeParticleCompute(commandBuffer, frameGraph);
        
        uint32_t counter = computeCounter.load();
        if (counter % 300 == 0) {
            std::cout << "SunParticleComputeNode: Executed compute for " << maxParticles << " particles" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "SunParticleComputeNode::execute exception: " << e.what() << std::endl;
    }
}

void SunParticleComputeNode::executeParticleCompute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        const auto& vk = vulkanContext->getLoader();
        
        // Create pipeline state with sun system layout
        DescriptorLayoutSpec layoutSpec = DescriptorLayoutPresets::createSunSystemLayout();
        VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
        ComputePipelineState pipelineState = ComputePipelinePresets::createSunParticleState(descriptorLayout);
        
        // Get pipeline
        VkPipeline pipeline = computeManager->getPipeline(pipelineState);
        VkPipelineLayout pipelineLayout = computeManager->getPipelineLayout(pipelineState);
        
        if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
            std::cerr << "SunParticleComputeNode: Failed to get compute pipeline" << std::endl;
            return;
        }
        
        // Bind pipeline
        vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        
        // TODO: Bind descriptor sets with UBO and particle buffer
        
        // Calculate dispatch parameters
        uint32_t workgroupSize = 64; // From shader local_size_x
        uint32_t numWorkgroups = (maxParticles + workgroupSize - 1) / workgroupSize;
        
        // Dispatch compute shader
        vk.vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);
        
    } catch (const std::exception& e) {
        std::cerr << "SunParticleComputeNode::executeParticleCompute exception: " << e.what() << std::endl;
    }
}