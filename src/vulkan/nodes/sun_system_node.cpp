#include "sun_system_node.h"
#include "../pipelines/graphics_pipeline_manager.h"
#include "../pipelines/compute_pipeline_manager.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../core/vulkan_swapchain.h"
#include "../resources/core/resource_coordinator.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../../ecs/core/service_locator.h"
#include "../../ecs/services/camera_service.h"
#include <iostream>
#include <array>
#include <stdexcept>

SunSystemNode::SunSystemNode(
    GraphicsPipelineManager* graphicsManager,
    ComputePipelineManager* computeManager,
    VulkanSwapchain* swapchain,
    ResourceCoordinator* resourceCoordinator
) : graphicsManager(graphicsManager)
  , computeManager(computeManager)
  , swapchain(swapchain)
  , resourceCoordinator(resourceCoordinator) {
    
    if (!graphicsManager) {
        throw std::invalid_argument("SunSystemNode: graphicsManager cannot be null");
    }
    if (!computeManager) {
        throw std::invalid_argument("SunSystemNode: computeManager cannot be null");
    }
    if (!swapchain) {
        throw std::invalid_argument("SunSystemNode: swapchain cannot be null");
    }
    if (!resourceCoordinator) {
        throw std::invalid_argument("SunSystemNode: resourceCoordinator cannot be null");
    }
    
    std::cout << "SunSystemNode: Creating sun system with " << maxParticles << " particles" << std::endl;
}

SunSystemNode::~SunSystemNode() {
    if (descriptorPool != VK_NULL_HANDLE && vulkanContext) {
        const auto& vk = vulkanContext->getLoader();
        const VkDevice device = vulkanContext->getDevice();
        vk.vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (quadVertexBuffer != VK_NULL_HANDLE) {
        // ResourceCoordinator handles cleanup of buffers
    }
}

std::vector<ResourceDependency> SunSystemNode::getInputs() const {
    std::vector<ResourceDependency> inputs;
    
    // Particle buffer for compute and graphics
    if (particleBufferId != 0) {
        inputs.push_back({particleBufferId, ResourceAccess::ReadWrite, PipelineStage::ComputeShader});
        inputs.push_back({particleBufferId, ResourceAccess::Read, PipelineStage::VertexShader});
    }
    
    // Swapchain image for rendering
    if (currentSwapchainImageId != 0) {
        inputs.push_back({currentSwapchainImageId, ResourceAccess::Read, PipelineStage::ColorAttachment});
    }
    
    return inputs;
}

std::vector<ResourceDependency> SunSystemNode::getOutputs() const {
    std::vector<ResourceDependency> outputs;
    
    // Particle buffer modified by compute
    if (particleBufferId != 0) {
        outputs.push_back({particleBufferId, ResourceAccess::Write, PipelineStage::ComputeShader});
    }
    
    // Swapchain image rendered to
    if (currentSwapchainImageId != 0) {
        outputs.push_back({currentSwapchainImageId, ResourceAccess::Write, PipelineStage::ColorAttachment});
    }
    
    return outputs;
}

bool SunSystemNode::initializeNode(const FrameGraph& frameGraph) {
    try {
        vulkanContext = frameGraph.getContext();
        if (!vulkanContext) {
            std::cerr << "SunSystemNode: VulkanContext is null" << std::endl;
            return false;
        }
        
        // Create particle buffer (4 vec4s per particle = 64 bytes per particle)
        VkDeviceSize particleBufferSize = maxParticles * sizeof(SunParticle);
        
        // Create the particle buffer in the frame graph
        particleBufferId = const_cast<FrameGraph&>(frameGraph).createBuffer(
            "SunParticleBuffer",
            particleBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        );
        
        std::cout << "SunSystemNode: Created particle buffer ID: " << particleBufferId 
                  << " for " << maxParticles << " particles (" << particleBufferSize << " bytes)" << std::endl;
        
        if (!createParticleResources(frameGraph)) {
            std::cerr << "SunSystemNode: Failed to create particle resources" << std::endl;
            return false;
        }
        
        if (!createDescriptorResources(frameGraph)) {
            std::cerr << "SunSystemNode: Failed to create descriptor resources" << std::endl;
            return false;
        }
        
        resourcesInitialized = true;
        std::cout << "SunSystemNode: *** SUCCESSFULLY INITIALIZED SUN SYSTEM ***" << std::endl;
        std::cout << "  - Particle buffer ID: " << particleBufferId << std::endl;
        std::cout << "  - Max particles: " << maxParticles << std::endl;
        std::cout << "  - Resources initialized: " << resourcesInitialized << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::initializeNode exception: " << e.what() << std::endl;
        return false;
    }
}

void SunSystemNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    frameTime = time;
    frameDeltaTime = deltaTime;
    currentFrameIndex = frameIndex;
    
    updateSunUBO();
}

void SunSystemNode::releaseFrame(uint32_t frameIndex) {
    // No per-frame cleanup needed
}

void SunSystemNode::updateSunUBO() {
    // Get camera matrices
    if (world) {
        try {
            CameraService& cameraService = SERVICE(CameraService);
            sunUBO.viewMatrix = cameraService.getViewMatrix();
            sunUBO.projMatrix = cameraService.getProjectionMatrix();
            sunUBO.cameraPos = glm::vec4(cameraService.getCameraPosition(), 90.0f); // Default FOV
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
    
    // Update UBO buffer
    if (sunUBOHandle.buffer.get() != VK_NULL_HANDLE) {
        resourceCoordinator->copyToBuffer(sunUBOHandle, &sunUBO, sizeof(SunUBO));
    }
    
    uboNeedsUpdate = true;
}

void SunSystemNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        debugCounter++;
        
        // Always log execution for debugging
        uint32_t counter = debugCounter.load();
        if (counter % 60 == 0) { // Every second at 60 FPS
            std::cout << "SunSystemNode: EXECUTING sun system! Resources: " << resourcesInitialized 
                     << ", SwapchainID: " << currentSwapchainImageId 
                     << ", ParticleBufferID: " << particleBufferId << std::endl;
        }
        
        if (!resourcesInitialized) {
            std::cerr << "SunSystemNode: Resources not initialized, skipping execution" << std::endl;
            return;
        }
        
        // Skip if no valid swapchain image
        if (currentSwapchainImageId == 0) {
            if (counter % 300 == 0) {
                std::cout << "SunSystemNode: No valid swapchain image (" << currentSwapchainImageId << "), skipping" << std::endl;
            }
            return;
        }
        
        // Execute compute pass first to update particles
        executeParticleCompute(commandBuffer, frameGraph);
        
        // Add compute-to-graphics barrier
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        const auto& vk = vulkanContext->getLoader();
        vk.vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr
        );
        
        // Execute graphics pass to render sun and particles
        executeGraphicsRender(commandBuffer, frameGraph);
        
        if (counter % 1800 == 0) { // Every 30 seconds at 60 FPS
            std::cout << "SunSystemNode: Sun system stats - compute: " << computeCounter.load() 
                     << ", render: " << renderCounter.load() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::execute exception: " << e.what() << std::endl;
    }
}

bool SunSystemNode::createParticleResources(const FrameGraph& frameGraph) {
    // Create fullscreen quad for sun disc and particle quads
    std::vector<glm::vec2> quadVertices = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f},
        {1.0f, 1.0f}, {-1.0f, 1.0f}, {-1.0f, -1.0f}
    };
    
    VkDeviceSize quadBufferSize = quadVertices.size() * sizeof(glm::vec2);
    
    ResourceHandle quadHandle = resourceCoordinator->createBuffer(
        quadBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    resourceCoordinator->copyToBuffer(quadHandle, quadVertices.data(), quadBufferSize);
    quadVertexBuffer = quadHandle.buffer.get();
    
    // Create sun UBO
    VkDeviceSize uboSize = sizeof(SunUBO);
    sunUBOHandle = resourceCoordinator->createBuffer(
        uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    std::cout << "SunSystemNode: Created particle resources (quad buffer and UBO)" << std::endl;
    return true;
}

bool SunSystemNode::createDescriptorResources(const FrameGraph& frameGraph) {
    const auto& vk = vulkanContext->getLoader();
    const VkDevice device = vulkanContext->getDevice();
    
    // Create descriptor pool (UBO + storage buffer for both compute and graphics)
    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2; // One for compute, one for graphics
    
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 2; // One for compute, one for graphics
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 2; // Compute and graphics sets
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    
    VkResult result = vk.vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        std::cerr << "SunSystemNode: Failed to create descriptor pool" << std::endl;
        return false;
    }
    
    // TODO: Create and allocate descriptor sets
    // This would require proper descriptor layouts from the pipeline managers
    
    std::cout << "SunSystemNode: Created descriptor resources" << std::endl;
    return true;
}

void SunSystemNode::executeParticleCompute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    computeCounter++;
    
    try {
        // Calculate dispatch parameters
        uint32_t workgroupSize = 64; // From shader local_size_x
        uint32_t numWorkgroups = (maxParticles + workgroupSize - 1) / workgroupSize;
        
        // For now, just log that we would dispatch
        uint32_t counter = computeCounter.load();
        if (counter % 300 == 0) {
            std::cout << "SunSystemNode: Would dispatch " << numWorkgroups << " workgroups for " << maxParticles << " particles" << std::endl;
        }
        
        // TODO: Actual pipeline creation and dispatch once descriptor layouts are ready
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::executeParticleCompute exception: " << e.what() << std::endl;
    }
}

void SunSystemNode::executeGraphicsRender(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    renderCounter++;
    
    try {
        // Log progress
        uint32_t counter = renderCounter.load();
        if (counter % 300 == 0) {
            std::cout << "SunSystemNode: Graphics render pass - sun and particles rendering (placeholder)" << std::endl;
        }
        
        // TODO: Implement actual graphics rendering once pipeline system integration is complete
        // For now, just track that we're being called
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::executeGraphicsRender exception: " << e.what() << std::endl;
    }
}