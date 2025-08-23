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
#include <vector>

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

void EntityGraphicsNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    
    // Validate dependencies are still valid
    if (!graphicsManager || !swapchain || !resourceCoordinator || !gpuEntityManager) {
        std::cerr << "EntityGraphicsNode: Critical error - dependencies became null during execution" << std::endl;
        return;
    }
    
    const uint32_t entityCount = gpuEntityManager->getEntityCount();
    
    if (entityCount == 0) {
        uint32_t counter = noEntitiesCounter.fetch_add(1, std::memory_order_relaxed);
        if (counter % 1800 == 0) {
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
    
    // Get the render pass with depth buffer for proper 3D rendering
    VkRenderPass renderPass = graphicsManager->createRenderPass(
        swapchain->getImageFormat(), 
        VK_FORMAT_D24_UNORM_S8_UINT,  // Depth buffer for 3D cubes
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

    // Clear values: MSAA color, resolve color, depth buffer
    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = {{0.02f, 0.02f, 0.08f, 1.0f}};  // MSAA color attachment - deep space black
    clearValues[1].color = {{0.02f, 0.02f, 0.08f, 1.0f}};  // Resolve attachment - deep space black  
    clearValues[2].depthStencil = {1.0f, 0};             // Depth buffer (1.0 = far plane)
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
            resourceCoordinator->getGraphicsManager()->getVertexBuffer()      // Vertex positions for cube geometry
        };
        VkDeviceSize offsets[] = {0};
        vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Bind index buffer for cube geometry
        vk.vkCmdBindIndexBuffer(
            commandBuffer, resourceCoordinator->getGraphicsManager()->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
        
        // Draw indexed instances: all entities with cube geometry
        vk.vkCmdDrawIndexed(
            commandBuffer, 
            resourceCoordinator->getGraphicsManager()->getIndexCount(),  // Number of indices per cube
            entityCount,                      // Number of instances (entities)
            0, 0, 0                          // Index/vertex/instance offsets
        );
        
        // Debug: confirm draw call (thread-safe)
        uint32_t counter = drawCounter.fetch_add(1, std::memory_order_relaxed);
        if (counter % 1800 == 0) {
            std::cout << "EntityGraphicsNode: Drew " << entityCount 
                      << " entities with " << resourceCoordinator->getGraphicsManager()->getIndexCount() 
                      << " indices per cube" << std::endl;
        }
    }

    // === SUN SYSTEM RENDERING (within same render pass) ===
    renderSunSystem(commandBuffer, vk);

    // End render pass
    vk.vkCmdEndRenderPass(commandBuffer);
}

void EntityGraphicsNode::updateUniformBuffer() {
    if (!resourceCoordinator) return;
    
    // Check if uniform buffer needs updating for this frame index
    bool needsUpdate = uniformBufferDirty || (lastUpdatedFrameIndex != currentFrameIndex);
    
    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 lightSpaceMatrix0;
        glm::mat4 lightSpaceMatrix1;
        glm::mat4 lightSpaceMatrix2;
        glm::vec4 sunDirection;       
        glm::vec4 cascadeSplits;      
        float shadowDistance;
        uint32_t cascadeCount;
        float shadowBias;
        float shadowNormalOffset;
    } newUBO{};

    // Get camera matrices from service
    auto& cameraService = ServiceLocator::instance().requireService<CameraService>();
    newUBO.view = cameraService.getViewMatrix();
    newUBO.proj = cameraService.getProjectionMatrix();
    
    // Shadow mapping parameters - temporary placeholder values
    // TODO: Get actual light space matrices from shadow pass node
    newUBO.lightSpaceMatrix0 = glm::mat4(1.0f);
    newUBO.lightSpaceMatrix1 = glm::mat4(1.0f);  
    newUBO.lightSpaceMatrix2 = glm::mat4(1.0f);
    // Use the SAME sun direction as the shadow system for consistency
    glm::vec3 sunDir = sunDirection; // Use the class member variable
    newUBO.sunDirection = glm::vec4(sunDir, 3.0f); // w = intensity
    newUBO.cascadeSplits = glm::vec4(0.1f, 0.3f, 1.0f, 1.0f);
    newUBO.shadowDistance = 1000.0f;
    newUBO.cascadeCount = 3;
    newUBO.shadowBias = 0.005f;
    newUBO.shadowNormalOffset = 0.1f;
    
    // Debug camera matrix application (once every 30 seconds) - thread-safe
    uint32_t counter = debugCounter.fetch_add(1, std::memory_order_relaxed);
    if (counter % 1800 == 0) {
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
        if (counter % 1800 == 0) {
            std::cout << "EntityGraphicsNode: Using fallback matrices - no world reference" << std::endl;
        }
    }
    
    // Check if matrices actually changed (simplified for shadow mapping - always update for now)
    bool matricesChanged = true; // TODO: Implement proper caching for extended UBO
    
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
                // TODO: Update cached UBO with full structure
                cachedUBO.view = newUBO.view;
                cachedUBO.proj = newUBO.proj;
                uniformBufferDirty = false;
                lastUpdatedFrameIndex = currentFrameIndex;
                
                // Debug optimized updates (once every 30 seconds) - thread-safe
                uint32_t counter = updateCounter.fetch_add(1, std::memory_order_relaxed);
                if (counter % 1800 == 0) {
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
    if (!resourceCoordinator) {
        std::cerr << "EntityGraphicsNode: ResourceCoordinator is null" << std::endl;
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

void EntityGraphicsNode::renderSunSystem(VkCommandBuffer commandBuffer, const VulkanFunctionLoader& vk) {
    // Initialize sun resources if not already done
    if (!sunResourcesInitialized) {
        if (!initializeSunResources()) {
            static uint32_t errorCounter = 0;
            errorCounter++;
            if (errorCounter % 300 == 0) {
                std::cerr << "EntityGraphicsNode: Failed to initialize sun resources!" << std::endl;
            }
            return;
        }
    }
    
    // Validate resources
    if (sunQuadBuffer == VK_NULL_HANDLE || !graphicsManager) {
        return;
    }
    
    // Use the same render pass that the entity graphics are using
    VkRenderPass currentRenderPass = graphicsManager->createRenderPass(
        swapchain->getImageFormat(), 
        VK_FORMAT_D24_UNORM_S8_UINT,  // Same depth format as entity rendering
        VK_SAMPLE_COUNT_2_BIT,         // Same MSAA as entity rendering  
        true                           // Enable depth testing
    );
    
    // Get the same descriptor layout used by entity rendering (has UBO binding)
    auto layoutSpec = DescriptorLayoutPresets::createEntityGraphicsLayout();
    VkDescriptorSetLayout entityDescriptorLayout = graphicsManager->getLayoutManager()->getLayout(layoutSpec);
    
    // Create sun pipeline state compatible with current render pass and using same UBO  
    GraphicsPipelineState sunPipelineState = GraphicsPipelinePresets::createSunSystemRenderingState(
        currentRenderPass, 
        entityDescriptorLayout
    );
    
    // Get pipeline
    VkPipeline sunPipeline = graphicsManager->getPipeline(sunPipelineState);
    
    if (sunPipeline == VK_NULL_HANDLE) {
        static uint32_t pipelineErrorCounter = 0;
        pipelineErrorCounter++;
        if (pipelineErrorCounter % 300 == 0) {
            std::cerr << "EntityGraphicsNode: Failed to get sun pipeline!" << std::endl;
        }
        return;
    }
    
    // Bind sun pipeline
    vk.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sunPipeline);
    
    // Bind the same descriptor set as entity rendering (contains UBO with camera matrices)
    const auto& descriptorSets = resourceCoordinator->getGraphicsManager()->getDescriptorSets();
    VkDescriptorSet entityDescriptorSet = !descriptorSets.empty() ? descriptorSets[currentFrameIndex % descriptorSets.size()] : VK_NULL_HANDLE;
    if (entityDescriptorSet != VK_NULL_HANDLE) {
        VkPipelineLayout sunPipelineLayout = graphicsManager->getPipelineLayout(sunPipelineState);
        vk.vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            sunPipelineLayout,
            0, 1, &entityDescriptorSet,
            0, nullptr
        );
    }
    
    // Bind sun vertex buffer
    VkBuffer vertexBuffers[] = { sunQuadBuffer };
    VkDeviceSize offsets[] = { 0 };
    vk.vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
    // Draw sun disc only (6 vertices for two triangles making a quad)
    vk.vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    
    // TODO: Add particle rendering later - for now just show sun disc
    
    // Debug logging - more frequent to see if it's working
    static uint32_t sunCounter = 0;
    sunCounter++;
    if (sunCounter % 60 == 0) {
        std::cout << "EntityGraphicsNode: Sun system active - pipeline=" << (sunPipeline != VK_NULL_HANDLE) 
                  << " buffer=" << (sunQuadBuffer != VK_NULL_HANDLE) << std::endl;
    }
}

bool EntityGraphicsNode::initializeSunResources() {
    if (sunResourcesInitialized) {
        return true;
    }
    
    if (!resourceCoordinator || !resourceCoordinator->getContext()) {
        std::cerr << "EntityGraphicsNode: Cannot initialize sun resources - missing resource coordinator or context!" << std::endl;
        return false;
    }
    
    const auto& vk = resourceCoordinator->getContext()->getLoader();
    VkDevice device = resourceCoordinator->getContext()->getDevice();
    
    // Create simple quad vertex data (2 triangles = 6 vertices)
    // Each vertex is just a 2D position (vec2)
    std::vector<glm::vec2> quadVertices = {
        // First triangle
        {-1.0f, -1.0f},  // Bottom left
        { 1.0f, -1.0f},  // Bottom right  
        { 1.0f,  1.0f},  // Top right
        
        // Second triangle  
        {-1.0f, -1.0f},  // Bottom left
        { 1.0f,  1.0f},  // Top right
        {-1.0f,  1.0f}   // Top left
    };
    
    VkDeviceSize bufferSize = sizeof(glm::vec2) * quadVertices.size();
    
    // Create vertex buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = vk.vkCreateBuffer(device, &bufferInfo, nullptr, &sunQuadBuffer);
    if (result != VK_SUCCESS) {
        std::cerr << "EntityGraphicsNode: Failed to create sun quad vertex buffer! VkResult=" << result << std::endl;
        return false;
    }
    
    // Allocate memory for vertex buffer
    VkMemoryRequirements memRequirements;
    vk.vkGetBufferMemoryRequirements(device, sunQuadBuffer, &memRequirements);
    
    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memProperties;
    vk.vkGetPhysicalDeviceMemoryProperties(resourceCoordinator->getContext()->getPhysicalDevice(), &memProperties);
    
    uint32_t memoryTypeIndex = UINT32_MAX;
    VkMemoryPropertyFlags requiredProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & requiredProps) == requiredProps) {
            memoryTypeIndex = i;
            break;
        }
    }
    
    if (memoryTypeIndex == UINT32_MAX) {
        std::cerr << "EntityGraphicsNode: Failed to find suitable memory type for sun quad buffer!" << std::endl;
        vk.vkDestroyBuffer(device, sunQuadBuffer, nullptr);
        sunQuadBuffer = VK_NULL_HANDLE;
        return false;
    }
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    result = vk.vkAllocateMemory(device, &allocInfo, nullptr, &sunQuadMemory);
    if (result != VK_SUCCESS) {
        std::cerr << "EntityGraphicsNode: Failed to allocate sun quad memory! VkResult=" << result << std::endl;
        vk.vkDestroyBuffer(device, sunQuadBuffer, nullptr);
        sunQuadBuffer = VK_NULL_HANDLE;
        return false;
    }
    
    // Bind memory to buffer
    result = vk.vkBindBufferMemory(device, sunQuadBuffer, sunQuadMemory, 0);
    if (result != VK_SUCCESS) {
        std::cerr << "EntityGraphicsNode: Failed to bind sun quad memory! VkResult=" << result << std::endl;
        vk.vkFreeMemory(device, sunQuadMemory, nullptr);
        vk.vkDestroyBuffer(device, sunQuadBuffer, nullptr);
        sunQuadBuffer = VK_NULL_HANDLE;
        sunQuadMemory = VK_NULL_HANDLE;
        return false;
    }
    
    // Copy vertex data to buffer
    void* data;
    result = vk.vkMapMemory(device, sunQuadMemory, 0, bufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        std::cerr << "EntityGraphicsNode: Failed to map sun quad memory! VkResult=" << result << std::endl;
        cleanupSunResources();
        return false;
    }
    
    memcpy(data, quadVertices.data(), bufferSize);
    vk.vkUnmapMemory(device, sunQuadMemory);
    
    sunResourcesInitialized = true;
    std::cout << "EntityGraphicsNode: Successfully initialized sun quad vertex buffer" << std::endl;
    return true;
}

void EntityGraphicsNode::cleanupSunResources() {
    if (!resourceCoordinator || !resourceCoordinator->getContext()) {
        return;
    }
    
    const auto& vk = resourceCoordinator->getContext()->getLoader();
    VkDevice device = resourceCoordinator->getContext()->getDevice();
    
    if (sunQuadMemory != VK_NULL_HANDLE) {
        vk.vkFreeMemory(device, sunQuadMemory, nullptr);
        sunQuadMemory = VK_NULL_HANDLE;
    }
    
    if (sunQuadBuffer != VK_NULL_HANDLE) {
        vk.vkDestroyBuffer(device, sunQuadBuffer, nullptr);
        sunQuadBuffer = VK_NULL_HANDLE;
    }
    
    sunResourcesInitialized = false;
}