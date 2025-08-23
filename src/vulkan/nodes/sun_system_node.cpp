#include "sun_system_node.h"
#include "sun_particle_compute_node.h"
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
        
        // TEMP: Skip frame graph particle buffer creation to isolate issue
        std::cout << "SunSystemNode: Skipping particle buffer creation in frame graph" << std::endl;
        particleBufferId = 0;  // No particle buffer
        
        // TEMP: Delay resource creation to avoid initialization race conditions
        // Resources will be created on first frame execution instead
        std::cout << "SunSystemNode: Delaying resource creation to avoid race conditions" << std::endl;
        
        // TEMP: Skip descriptor resource creation to avoid interference  
        // if (!createDescriptorResources(frameGraph)) {
        //     std::cerr << "SunSystemNode: Failed to create descriptor resources" << std::endl;
        //     return false;
        // }
        
        resourcesInitialized = false;  // Will create resources on first execution
        std::cout << "SunSystemNode: *** INITIALIZED - will create resources during execution ***" << std::endl;
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
        
        // Create resources on first execution to avoid race conditions
        std::cout << "SunSystemNode: Checking resourcesInitialized=" << resourcesInitialized << std::endl;
        if (!resourcesInitialized) {
            std::cout << "SunSystemNode: Creating resources on first execution (frame " << counter << ")" << std::endl;
            
            // No compute node needed - vertex shader approach uses static data
            std::cout << "SunSystemNode: Using vertex-shader-only particle system" << std::endl;
            
            if (!createParticleResources(frameGraph)) {
                std::cerr << "SunSystemNode: Failed to create resources during execution" << std::endl;
                return;
            }
            
            // TEMP: Skip descriptor creation when no particle buffer
            if (staticParticleHandle.buffer.get() != VK_NULL_HANDLE) {
                if (!createDescriptorResources(frameGraph)) {
                    std::cerr << "SunSystemNode: Failed to create descriptor resources during execution" << std::endl;
                    return;
                }
            } else {
                std::cout << "SunSystemNode: Skipping descriptor creation - no particle buffer" << std::endl;
            }
            
            resourcesInitialized = true;
            std::cout << "SunSystemNode: Resources created successfully during execution" << std::endl;
        }
        
        // Skip if no valid swapchain image
        if (currentSwapchainImageId == 0) {
            if (counter % 300 == 0) {
                std::cout << "SunSystemNode: No valid swapchain image (" << currentSwapchainImageId << "), skipping" << std::endl;
            }
            return;
        }
        
        // Compute is now handled by separate SunParticleComputeNode
        
        // Execute full sun and particle rendering
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
    std::cout << "SunSystemNode: ENTERING createParticleResources" << std::endl;
    // TEMP: Only create the absolute minimum - just quad buffer for sun disc
    // TEMP: Try simple triangle instead of quad
    std::vector<glm::vec2> quadVertices = {
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}
    };
    
    // Debug: Print vertex data
    std::cout << "SunSystemNode: Quad vertices:" << std::endl;
    for (size_t i = 0; i < quadVertices.size(); ++i) {
        std::cout << "  [" << i << "]: (" << quadVertices[i].x << ", " << quadVertices[i].y << ")" << std::endl;
    }
    
    VkDeviceSize quadBufferSize = quadVertices.size() * sizeof(glm::vec2);
    
    ResourceHandle quadHandle = resourceCoordinator->createBuffer(
        quadBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    resourceCoordinator->copyToBuffer(quadHandle, quadVertices.data(), quadBufferSize);
    quadVertexBuffer = quadHandle.buffer.get();
    
    std::cout << "SunSystemNode: Created quad vertex buffer: " << quadVertexBuffer << std::endl;
    if (quadVertexBuffer == VK_NULL_HANDLE) {
        std::cerr << "SunSystemNode: CRITICAL - Failed to create quad vertex buffer!" << std::endl;
    }
    
    // Add back UBO creation for shader uniforms
    VkDeviceSize uboSize = sizeof(SunUBO);
    sunUBOHandle = resourceCoordinator->createBuffer(
        uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    // TEMP: Skip particle buffer creation entirely - test sun disc only
    std::cout << "SunSystemNode: SKIPPING particle buffer creation to isolate memory issue" << std::endl;
    
    std::cout << "SunSystemNode: Created resources (quad buffer + UBO + particle init)" << std::endl;
    std::cout << "SunSystemNode: EXITING createParticleResources SUCCESS - quadBuffer=" << quadVertexBuffer << std::endl;
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
    
    // Get sun system descriptor layout
    DescriptorLayoutSpec layoutSpec = DescriptorLayoutPresets::createSunSystemLayout();
    VkDescriptorSetLayout descriptorLayout = computeManager->getLayoutManager()->getLayout(layoutSpec);
    
    if (descriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "SunSystemNode: Failed to get sun system descriptor layout" << std::endl;
        return false;
    }
    
    // Allocate compute descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorLayout;
    
    result = vk.vkAllocateDescriptorSets(device, &allocInfo, &computeDescriptorSet);
    if (result != VK_SUCCESS) {
        std::cerr << "SunSystemNode: Failed to allocate compute descriptor set: " << result << std::endl;
        return false;
    }
    
    // Allocate graphics descriptor set
    result = vk.vkAllocateDescriptorSets(device, &allocInfo, &graphicsDescriptorSet);
    if (result != VK_SUCCESS) {
        std::cerr << "SunSystemNode: Failed to allocate graphics descriptor set: " << result << std::endl;
        return false;
    }
    
    // Use static particle buffer (no frame graph needed)
    VkBuffer particleBuffer = staticParticleHandle.buffer.get();
    if (particleBuffer == VK_NULL_HANDLE) {
        std::cerr << "SunSystemNode: Static particle buffer is null" << std::endl;
        return false;
    }
    
    // Update descriptor sets
    VkDescriptorBufferInfo uboBufferInfo{};
    uboBufferInfo.buffer = sunUBOHandle.buffer.get();
    uboBufferInfo.offset = 0;
    uboBufferInfo.range = sizeof(SunUBO);
    
    VkDescriptorBufferInfo particleBufferInfo{};
    particleBufferInfo.buffer = particleBuffer;
    particleBufferInfo.offset = 0;
    particleBufferInfo.range = VK_WHOLE_SIZE;
    
    VkWriteDescriptorSet descriptorWrites[4]; // 2 bindings × 2 sets = 4 writes
    
    // Compute descriptor set - binding 0: UBO
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].pNext = nullptr;
    descriptorWrites[0].dstSet = computeDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uboBufferInfo;
    descriptorWrites[0].pImageInfo = nullptr;
    descriptorWrites[0].pTexelBufferView = nullptr;
    
    // Compute descriptor set - binding 1: particle buffer
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].pNext = nullptr;
    descriptorWrites[1].dstSet = computeDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &particleBufferInfo;
    descriptorWrites[1].pImageInfo = nullptr;
    descriptorWrites[1].pTexelBufferView = nullptr;
    
    // Graphics descriptor set - binding 0: UBO
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].pNext = nullptr;
    descriptorWrites[2].dstSet = graphicsDescriptorSet;
    descriptorWrites[2].dstBinding = 0;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &uboBufferInfo;
    descriptorWrites[2].pImageInfo = nullptr;
    descriptorWrites[2].pTexelBufferView = nullptr;
    
    // Graphics descriptor set - binding 1: particle buffer
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].pNext = nullptr;
    descriptorWrites[3].dstSet = graphicsDescriptorSet;
    descriptorWrites[3].dstBinding = 1;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &particleBufferInfo;
    descriptorWrites[3].pImageInfo = nullptr;
    descriptorWrites[3].pTexelBufferView = nullptr;
    
    vk.vkUpdateDescriptorSets(device, 4, descriptorWrites, 0, nullptr);
    
    std::cout << "SunSystemNode: Created descriptor resources and allocated descriptor sets" << std::endl;
    return true;
}

void SunSystemNode::executeParticleCompute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    computeCounter++;
    
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
            uint32_t counter = computeCounter.load();
            if (counter % 300 == 0) {
                std::cerr << "SunSystemNode: Failed to get pipeline or layout for compute" << std::endl;
            }
            return;
        }
        
        // Bind pipeline
        vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        
        // Bind descriptor sets
        vk.vkCmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout,
            0, 1, &computeDescriptorSet, 0, nullptr
        );
        
        // Calculate dispatch parameters
        uint32_t workgroupSize = 64; // From shader local_size_x
        uint32_t numWorkgroups = (maxParticles + workgroupSize - 1) / workgroupSize;
        
        // Dispatch compute shader
        vk.vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);
        
        // Log progress periodically
        uint32_t counter = computeCounter.load();
        if (counter % 300 == 0) {
            std::cout << "SunSystemNode: Dispatched " << numWorkgroups << " workgroups for " << maxParticles << " particles" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::executeParticleCompute exception: " << e.what() << std::endl;
    }
}

void SunSystemNode::executeGraphicsRender(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    renderCounter++;
    
    try {
        const auto& vk = vulkanContext->getLoader();
        
        // Use the current render pass from context (don't create our own!)
        VkRenderPass renderPass = VK_NULL_HANDLE;
        // TODO: Get current render pass from frame graph or context
        // For now, create compatible render pass (same format as entities)
        renderPass = graphicsManager->createRenderPass(
            swapchain->getImageFormat(), 
            VK_FORMAT_D24_UNORM_S8_UINT,  // Must match entity render pass
            VK_SAMPLE_COUNT_2_BIT         // Must match entity render pass
        );
        
        if (renderPass == VK_NULL_HANDLE) {
            std::cerr << "SunSystemNode: Failed to create compatible render pass" << std::endl;
            return;
        }
        
        std::cout << "SunSystemNode: Created render pass " << renderPass << " (should match entity pass)" << std::endl;
        
        // Create pipeline state with sun system layout
        DescriptorLayoutSpec layoutSpec = DescriptorLayoutPresets::createSunSystemLayout();
        VkDescriptorSetLayout descriptorLayout = graphicsManager->getLayoutManager()->getLayout(layoutSpec);
        GraphicsPipelineState pipelineState = GraphicsPipelinePresets::createSunSystemRenderingState(renderPass, descriptorLayout);
        
        // Get pipeline
        VkPipeline pipeline = graphicsManager->getPipeline(pipelineState);
        VkPipelineLayout pipelineLayout = graphicsManager->getPipelineLayout(pipelineState);
        
        if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
            std::cerr << "SunSystemNode: CRITICAL - Pipeline or layout is NULL! Pipeline=" 
                     << pipeline << " Layout=" << pipelineLayout << std::endl;
            return;
        }
        
        std::cout << "SunSystemNode: Using pipeline=" << pipeline << " layout=" << pipelineLayout << std::endl;
        
        // Bind pipeline
        vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        
        // TEMP: Skip descriptor set binding to isolate UBO issues
        // std::cout << "SunSystemNode: Binding descriptor set=" << graphicsDescriptorSet << std::endl;
        // if (graphicsDescriptorSet == VK_NULL_HANDLE) {
        //     std::cerr << "SunSystemNode: CRITICAL - Graphics descriptor set is NULL!" << std::endl;
        //     return;
        // }
        // vk.vkCmdBindDescriptorSets(
        //     commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
        //     0, 1, &graphicsDescriptorSet, 0, nullptr
        // );
        std::cout << "SunSystemNode: SKIPPED descriptor binding to test minimal rendering" << std::endl;
        
        // Bind quad vertex buffer
        VkBuffer vertexBuffers[] = {quadVertexBuffer};
        VkDeviceSize offsets[] = {0};
        std::cout << "SunSystemNode: BINDING VERTEX BUFFER - buffer=" << quadVertexBuffer << std::endl;
        if (quadVertexBuffer == VK_NULL_HANDLE) {
            std::cerr << "SunSystemNode: ERROR - Quad vertex buffer is NULL!" << std::endl;
            return;
        }
        vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // TEMP: Skip push constants for minimal shader
        // struct PushConstants {
        //     int renderMode;
        //     int instanceId;
        // } pushConstants;
        // 
        // pushConstants.renderMode = 0;  // Sun disc
        // pushConstants.instanceId = 0;
        // vk.vkCmdPushConstants(commandBuffer, pipelineLayout, 
        //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
        //     0, sizeof(PushConstants), &pushConstants);
        std::cout << "SunSystemNode: SKIPPED push constants for minimal test" << std::endl;
        
        // TEMP: Skip draw call to test if render pass is the issue
        std::cout << "SunSystemNode: SKIPPING draw call - testing render pass issue" << std::endl;
        // vk.vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        
        // TEMP: Skip particle rendering to isolate sun disc issue
        // pushConstants.renderMode = 1;  // Particles
        // vk.vkCmdPushConstants(commandBuffer, pipelineLayout, 
        //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
        //     0, sizeof(PushConstants), &pushConstants);
        // 
        // // Draw particles (instanced - 6 vertices per quad, maxParticles instances)
        // vk.vkCmdDraw(commandBuffer, 6, maxParticles, 0, 0);
        
        std::cout << "SunSystemNode: Skipped particle rendering to test sun disc only" << std::endl;
        
        // Log progress periodically
        uint32_t counter = renderCounter.load();
        if (counter % 300 == 0) {
            std::cout << "SunSystemNode: Rendered sun disc and " << maxParticles << " particles" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::executeGraphicsRender exception: " << e.what() << std::endl;
    }
}

void SunSystemNode::executeSimplifiedSunRender(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        const auto& vk = vulkanContext->getLoader();
        
        // Create render pass 
        VkRenderPass renderPass = graphicsManager->createRenderPass(
            swapchain->getImageFormat(), 
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_SAMPLE_COUNT_2_BIT
        );
        
        if (renderPass == VK_NULL_HANDLE) {
            std::cerr << "SunSystemNode: Failed to create render pass for simplified sun" << std::endl;
            return;
        }
        
        // Use a simple UBO-only descriptor layout (no particle buffer)
        DescriptorLayoutSpec layoutSpec;
        layoutSpec.bindings.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT});
        
        VkDescriptorSetLayout descriptorLayout = graphicsManager->getLayoutManager()->getLayout(layoutSpec);
        
        // Create simplified graphics pipeline state manually
        GraphicsPipelineState pipelineState{};
        pipelineState.renderPass = renderPass;
        pipelineState.descriptorSetLayouts.push_back(descriptorLayout);
        
        // Use sun disc shaders
        pipelineState.shaderStages = {
            "shaders/sun_disc.vert.spv",
            "shaders/sun_disc.frag.spv"
        };
        
        // Vertex input for 2D quad
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(glm::vec2);
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        pipelineState.vertexBindings.push_back(vertexBinding);
        
        VkVertexInputAttributeDescription posAttr{};
        posAttr.binding = 0;
        posAttr.location = 0;
        posAttr.format = VK_FORMAT_R32G32_SFLOAT;
        posAttr.offset = 0;
        pipelineState.vertexAttributes.push_back(posAttr);
        
        // Configure blending for transparency
        VkPipelineColorBlendAttachmentState colorBlend{};
        colorBlend.blendEnable = VK_TRUE;
        colorBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlend.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlend.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        pipelineState.colorBlendAttachments.push_back(colorBlend);
        
        // Get pipeline
        VkPipeline pipeline = graphicsManager->getPipeline(pipelineState);
        VkPipelineLayout pipelineLayout = graphicsManager->getPipelineLayout(pipelineState);
        
        if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
            std::cerr << "SunSystemNode: Failed to get simplified sun pipeline" << std::endl;
            return;
        }
        
        // Bind pipeline
        vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        
        // TODO: Create and bind UBO descriptor set
        // For now, just bind vertex buffer and draw without descriptor set
        VkBuffer vertexBuffers[] = {quadVertexBuffer};
        VkDeviceSize offsets[] = {0};
        vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Draw sun disc (6 vertices for fullscreen quad)
        vk.vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        
        std::cout << "SunSystemNode: Rendered simplified sun disc" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "SunSystemNode::executeSimplifiedSunRender exception: " << e.what() << std::endl;
    }
}