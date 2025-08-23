#include "particle_graphics_node.h"
#include "../pipelines/graphics_pipeline_manager.h"
#include "../pipelines/descriptor_layout_manager.h"
#include "../core/vulkan_swapchain.h"
#include "../resources/core/resource_coordinator.h"
#include "../resources/core/resource_handle.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_function_loader.h"
#include "../../ecs/core/service_locator.h"
#include "../../ecs/services/camera_service.h"
#include <iostream>
#include <array>
#include <flecs.h>
#include <stdexcept>

ParticleGraphicsNode::~ParticleGraphicsNode() {
    if (descriptorPool != VK_NULL_HANDLE && vulkanContext) {
        const auto& vk = vulkanContext->getLoader();
        const VkDevice device = vulkanContext->getDevice();
        vk.vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
        descriptorSet = VK_NULL_HANDLE; // Destroyed with pool
    }
}

ParticleGraphicsNode::ParticleGraphicsNode(
    FrameGraphTypes::ResourceId particleBuffer,
    FrameGraphTypes::ResourceId colorTarget,
    GraphicsPipelineManager* graphicsManager,
    VulkanSwapchain* swapchain,
    ResourceCoordinator* resourceCoordinator
) : particleBufferId(particleBuffer)
  , colorTargetId(colorTarget)
  , graphicsManager(graphicsManager)
  , swapchain(swapchain)
  , resourceCoordinator(resourceCoordinator) {
    
    // Validate dependencies during construction for fail-fast behavior
    if (!graphicsManager) {
        throw std::invalid_argument("ParticleGraphicsNode: graphicsManager cannot be null");
    }
    if (!swapchain) {
        throw std::invalid_argument("ParticleGraphicsNode: swapchain cannot be null");
    }
    if (!resourceCoordinator) {
        throw std::invalid_argument("ParticleGraphicsNode: resourceCoordinator cannot be null");
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

std::vector<ResourceDependency> ParticleGraphicsNode::getInputs() const {
    std::vector<ResourceDependency> inputs = {
        {particleBufferId, ResourceAccess::Read, PipelineStage::VertexShader}
    };
    
    // Only add swapchain dependency if we have a valid resource ID
    if (currentSwapchainImageId != 0) {
        inputs.push_back({currentSwapchainImageId, ResourceAccess::Read, PipelineStage::ColorAttachment});
    }
    
    return inputs;
}

std::vector<ResourceDependency> ParticleGraphicsNode::getOutputs() const {
    std::vector<ResourceDependency> outputs;
    
    // Only add swapchain dependency if we have a valid resource ID
    if (currentSwapchainImageId != 0) {
        outputs.push_back({currentSwapchainImageId, ResourceAccess::Write, PipelineStage::ColorAttachment});
    }
    
    return outputs;
}

bool ParticleGraphicsNode::initializeNode(const FrameGraph& frameGraph) {
    try {
        // Create quad vertex buffer for particle rendering
        // Each particle will be rendered as a quad (2 triangles, 6 vertices)
        std::vector<glm::vec2> quadVertices = {
            {-1.0f, -1.0f}, // Bottom-left
            { 1.0f, -1.0f}, // Bottom-right
            { 1.0f,  1.0f}, // Top-right
            { 1.0f,  1.0f}, // Top-right
            {-1.0f,  1.0f}, // Top-left
            {-1.0f, -1.0f}  // Bottom-left
        };
        
        VkDeviceSize quadBufferSize = quadVertices.size() * sizeof(glm::vec2);
        
        // Create buffer using ResourceCoordinator
        ResourceHandle quadHandle = resourceCoordinator->createBuffer(
            quadBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        
        // Upload quad vertices to buffer using ResourceCoordinator
        resourceCoordinator->copyToBuffer(quadHandle, quadVertices.data(), quadBufferSize);
        
        // Store the VkBuffer handle for use in rendering (we'll need the raw handle for binding)
        quadVertexBuffer = quadHandle.buffer.get();
        
        // Create particle UBO buffer
        VkDeviceSize uboSize = sizeof(ParticleUBO);
        particleUBOHandle = resourceCoordinator->createBuffer(
            uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        // Create descriptor pool and allocate descriptor set
        if (!createDescriptorResources(frameGraph)) {
            std::cerr << "ParticleGraphicsNode: Failed to create descriptor resources" << std::endl;
            return false;
        }
        
        std::cout << "ParticleGraphicsNode initialized for " << maxParticles << " particles" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ParticleGraphicsNode::initializeNode exception: " << e.what() << std::endl;
        return false;
    }
}

void ParticleGraphicsNode::prepareFrame(uint32_t frameIndex, float time, float deltaTime) {
    frameTime = time;
    frameDeltaTime = deltaTime;
    currentFrameIndex = frameIndex;
    
    // Update particle UBO data
    updateParticleUBO();
}

void ParticleGraphicsNode::releaseFrame(uint32_t frameIndex) {
    // No per-frame cleanup needed for particle graphics
}

void ParticleGraphicsNode::updateParticleUBO() {
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
    
    // Update the UBO buffer with new data
    if (particleUBOHandle.buffer.get() != VK_NULL_HANDLE) {
        resourceCoordinator->copyToBuffer(particleUBOHandle, &particleUBO, sizeof(ParticleUBO));
    }
    
    uboNeedsUpdate = true;
}

void ParticleGraphicsNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    try {
        debugCounter++;
        
        // TEMPORARILY: Try with entity shaders instead of particle shaders
        std::cout << "ParticleGraphicsNode: Testing with entity shaders" << std::endl;
        
        // Skip rendering if no swapchain image is set
        if (currentSwapchainImageId == 0) {
            // No valid swapchain image - skip particle rendering
            std::cout << "ParticleGraphicsNode: Skipping render - no valid swapchain image ID" << std::endl;
            return;
        }
        
        // Get Vulkan context and function loader from frame graph
        const VulkanContext* context = frameGraph.getContext();
        if (!context) {
            std::cerr << "ParticleGraphicsNode::execute: VulkanContext is null" << std::endl;
            return;
        }
        
        const auto& vk = context->getLoader();
        
        // TEMPORARY: Create minimal descriptor layout with one dummy binding for testing
        DescriptorLayoutSpec minimalLayoutSpec;
        minimalLayoutSpec.layoutName = "MinimalLayout";
        
        DescriptorBinding dummyBinding{};
        dummyBinding.binding = 0;
        dummyBinding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dummyBinding.descriptorCount = 1;
        dummyBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        dummyBinding.debugName = "dummyUBO";
        
        minimalLayoutSpec.bindings = {dummyBinding};
        VkDescriptorSetLayout descriptorLayout = graphicsManager->getLayoutManager()->getLayout(minimalLayoutSpec);
        std::cout << "ParticleGraphicsNode: Using minimal descriptor layout for testing" << std::endl;
        
        // Create render pass for particle rendering
        VkRenderPass renderPass = graphicsManager->createRenderPass(
            swapchain->getImageFormat(), 
            VK_FORMAT_D24_UNORM_S8_UINT,  // Depth buffer
            VK_SAMPLE_COUNT_2_BIT,        // MSAA samples
            true                          // Enable MSAA
        );
        
        // Use particle rendering state with blending disabled for debugging
        GraphicsPipelineState pipelineState = GraphicsPipelinePresets::createParticleRenderingState(
            renderPass, descriptorLayout);
        
        // TEMPORARY: Disable alpha blending to test if that's causing the crash
        if (!pipelineState.colorBlendAttachments.empty()) {
            pipelineState.colorBlendAttachments[0].blendEnable = VK_FALSE;
            std::cout << "ParticleGraphicsNode: Disabled alpha blending for debugging" << std::endl;
        }
        
        // Now try to create the pipeline - we know this is where the crash happens
        std::cout << "ParticleGraphicsNode: About to call getPipeline() - this is where the crash occurs" << std::endl;
        
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        
        try {
            pipeline = graphicsManager->getPipeline(pipelineState);
            std::cout << "ParticleGraphicsNode: getPipeline() succeeded, pipeline = " << std::hex << pipeline << std::endl;
            
            if (pipeline != VK_NULL_HANDLE) {
                pipelineLayout = graphicsManager->getPipelineLayout(pipelineState);
                std::cout << "ParticleGraphicsNode: getPipelineLayout() succeeded, layout = " << std::hex << pipelineLayout << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "ParticleGraphicsNode: CAUGHT EXCEPTION in pipeline creation: " << e.what() << std::endl;
            return;
        }
        
        if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
            std::cerr << "ParticleGraphicsNode: Failed to get particle graphics pipeline" << std::endl;
            std::cerr << "  pipeline = " << std::hex << pipeline << std::endl;  
            std::cerr << "  pipelineLayout = " << std::hex << pipelineLayout << std::endl;
            return;
        }
        
        std::cout << "ParticleGraphicsNode: Successfully created particle pipeline!" << std::endl;
        
        // Get swapchain image index for framebuffer (TODO: get from frame graph)
        uint32_t imageIndex = 0;
        const auto& framebuffers = swapchain->getFramebuffers();
        if (imageIndex >= framebuffers.size()) {
            std::cerr << "ParticleGraphicsNode: Invalid imageIndex " << imageIndex << std::endl;
            return;
        }
        
        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchain->getExtent();

        // Clear values (don't clear since particles are additive)
        std::array<VkClearValue, 3> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // Transparent
        clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // Transparent
        clearValues[2].depthStencil = {1.0f, 0};              // Far depth
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

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

        // Bind pipeline
        vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind quad vertex buffer
        if (quadVertexBuffer != VK_NULL_HANDLE) {
            VkDeviceSize offset = 0;
            vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, &quadVertexBuffer, &offset);
        }

        // TEMPORARY: Skip descriptor set binding for minimal shader test
        std::cout << "ParticleGraphicsNode: Skipping descriptor set binding for minimal test" << std::endl;
        
        // TODO: Draw instanced particles (maxParticles instances)
        // For now, draw 6 vertices (1 quad) to test basic pipeline creation
        vk.vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        
        vk.vkCmdEndRenderPass(commandBuffer);
        renderCounter++;
        
        uint32_t counter = debugCounter.load();
        if (counter % 3600 == 0) { // Every 60 seconds at 60 FPS
            std::cout << "ParticleGraphicsNode: Rendered particles " << renderCounter.load() << " times" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ParticleGraphicsNode::execute exception: " << e.what() << std::endl;
    }
}

bool ParticleGraphicsNode::createDescriptorResources(const FrameGraph& frameGraph) {
    vulkanContext = frameGraph.getContext();
    if (!vulkanContext) {
        std::cerr << "ParticleGraphicsNode::createDescriptorResources: VulkanContext is null" << std::endl;
        return false;
    }
    const VulkanContext* context = vulkanContext;
    
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    // Create descriptor pool for particle graphics (UBO + storage buffer)
    VkDescriptorPoolSize poolSizes[2];
    
    // Pool size for UBO (binding 0)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    
    // Pool size for storage buffer (binding 1)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0; // Could add VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT if needed
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    
    VkResult result = vk.vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        std::cerr << "ParticleGraphicsNode: Failed to create descriptor pool: " << result << std::endl;
        return false;
    }
    
    // Get particle graphics layout
    auto layoutSpec = DescriptorLayoutPresets::createParticleGraphicsLayout();
    VkDescriptorSetLayout descriptorLayout = graphicsManager->getLayoutManager()->getLayout(layoutSpec);
    
    if (descriptorLayout == VK_NULL_HANDLE) {
        std::cerr << "ParticleGraphicsNode: Failed to get particle graphics descriptor layout" << std::endl;
        return false;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorLayout;
    
    result = vk.vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        std::cerr << "ParticleGraphicsNode: Failed to allocate descriptor set: " << result << std::endl;
        return false;
    }
    
    // Get particle buffer from frame graph
    std::cout << "ParticleGraphicsNode: Getting particle buffer from frame graph (ID: " << particleBufferId << ")" << std::endl;
    VkBuffer particleBuffer = frameGraph.getBuffer(particleBufferId);
    std::cout << "ParticleGraphicsNode: Particle buffer handle: " << std::hex << particleBuffer << std::endl;
    
    if (particleBuffer == VK_NULL_HANDLE) {
        std::cerr << "ParticleGraphicsNode: ERROR - Particle buffer is null!" << std::endl;
        std::cerr << "  particleBufferId = " << particleBufferId << std::endl;
        return false;
    }
    
    // Update descriptor set with both UBO and particle buffer
    VkDescriptorBufferInfo uboBufferInfo{};
    uboBufferInfo.buffer = particleUBOHandle.buffer.get();
    uboBufferInfo.offset = 0;
    uboBufferInfo.range = sizeof(ParticleUBO);
    
    VkDescriptorBufferInfo particleBufferInfo{};
    particleBufferInfo.buffer = particleBuffer;
    particleBufferInfo.offset = 0;
    particleBufferInfo.range = VK_WHOLE_SIZE; // Use entire particle buffer
    
    VkWriteDescriptorSet descriptorWrites[2];
    
    // Binding 0: UBO
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].pNext = nullptr;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uboBufferInfo;
    descriptorWrites[0].pImageInfo = nullptr;
    descriptorWrites[0].pTexelBufferView = nullptr;
    
    // Binding 1: Particle buffer (storage buffer)
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].pNext = nullptr;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &particleBufferInfo;
    descriptorWrites[1].pImageInfo = nullptr;
    descriptorWrites[1].pTexelBufferView = nullptr;
    
    vk.vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
    
    std::cout << "ParticleGraphicsNode: Created descriptor pool and allocated descriptor set" << std::endl;
    return true;
}