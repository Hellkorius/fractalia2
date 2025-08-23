#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"
#include "../resources/core/resource_handle.h"
#include <flecs.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <atomic>

// Forward declarations
class GraphicsPipelineManager;
class ComputePipelineManager;
class VulkanSwapchain;
class ResourceCoordinator;

/**
 * Unified sun system node that renders:
 * 1. A bright sun disc in the sky
 * 2. Volumetric light rays emanating from the sun
 * 3. Floating light particles that drift through the scene
 */
class SunSystemNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(SunSystemNode)
    
public:
    SunSystemNode(
        GraphicsPipelineManager* graphicsManager,
        ComputePipelineManager* computeManager,
        VulkanSwapchain* swapchain,
        ResourceCoordinator* resourceCoordinator
    );
    
    ~SunSystemNode();
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements - graphics only (compute will be separate node)
    bool needsComputeQueue() const override { return false; }
    bool needsGraphicsQueue() const override { return true; }
    
    // Node lifecycle
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Configuration
    void setWorld(flecs::world* world) { this->world = world; }
    void setImageIndex(uint32_t imageIndex) { this->imageIndex = imageIndex; }
    void setCurrentSwapchainImageId(FrameGraphTypes::ResourceId currentImageId) { 
        this->currentSwapchainImageId = currentImageId; 
    }
    
    // Sun parameters
    void setSunPosition(const glm::vec3& position) { sunPosition = position; }
    void setSunColor(const glm::vec3& color) { sunColor = color; }
    void setSunIntensity(float intensity) { sunIntensity = intensity; }
    void setParticleCount(uint32_t count) { maxParticles = count; }
    
    // Access particle buffer for compute node
    FrameGraphTypes::ResourceId getParticleBufferId() const { return particleBufferId; }
    
    // Link to compute node for buffer sharing
    void setComputeNode(class SunParticleComputeNode* computeNode) { this->computeNode = computeNode; }

private:
    // Sun particle structure - simple and efficient
    struct SunParticle {
        glm::vec4 position;    // xyz = world position, w = life (0.0-1.0)
        glm::vec4 velocity;    // xyz = velocity, w = brightness
        glm::vec4 color;       // rgba = particle color with alpha
        glm::vec4 properties;  // x = size, y = age, z = type, w = spawn_timer
    };
    
    // Uniform buffer for sun system
    struct SunUBO {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec4 sunPosition;     // xyz = position, w = radius
        glm::vec4 sunColor;        // rgb = color, a = intensity  
        glm::vec4 cameraPos;       // xyz = camera position, w = fov
        glm::vec4 sceneInfo;       // x = time, y = deltaTime, z = particleCount, w = windStrength
        glm::vec4 lightParams;     // x = rayLength, y = rayIntensity, z = particleBrightness, w = gravityStrength
    };
    
    // Initialize particle buffer and descriptor resources
    bool createParticleResources(const FrameGraph& frameGraph);
    bool createDescriptorResources(const FrameGraph& frameGraph);
    
    // Update sun and particle parameters
    void updateSunUBO();
    
    // Execute compute pass to update particles
    void executeParticleCompute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph);
    
    // Execute graphics pass to render sun and particles
    void executeGraphicsRender(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph);
    
    // Execute simplified sun disc rendering (no particles, no particle buffer)
    void executeSimplifiedSunRender(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph);
    
    // Resources
    FrameGraphTypes::ResourceId particleBufferId = 0;  // Created internally
    FrameGraphTypes::ResourceId currentSwapchainImageId = 0; // Dynamic per-frame
    ResourceHandle sunUBOHandle;                       // Sun uniform buffer
    ResourceHandle staticParticleHandle;               // Static particle data for vertex shader
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE;       // Fullscreen quad for sun disc
    
    // Descriptor resources
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
    const VulkanContext* vulkanContext = nullptr;
    
    // External dependencies
    GraphicsPipelineManager* graphicsManager;
    ComputePipelineManager* computeManager;
    VulkanSwapchain* swapchain;
    ResourceCoordinator* resourceCoordinator;
    
    // Current frame state
    uint32_t imageIndex = 0;
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
    flecs::world* world = nullptr;
    
    // Sun configuration
    glm::vec3 sunPosition = glm::vec3(0.0f, 50.0f, 0.0f);  // High in the sky
    glm::vec3 sunColor = glm::vec3(1.0f, 0.9f, 0.7f);      // Warm sun color
    float sunIntensity = 2.5f;                             // Bright intensity
    float sunRadius = 3.0f;                                // Visual sun disc size
    
    // Particle parameters
    uint32_t maxParticles = 4;                             // MINIMAL for memory safety testing
    float particleLifetime = 10.0f;                        // 10 second lifetime
    float windStrength = 0.3f;                            // Gentle wind drift
    float gravityStrength = 0.1f;                         // Very light gravity
    float rayLength = 100.0f;                             // Length of light rays
    float rayIntensity = 1.5f;                            // Intensity of light rays
    
    // Internal state
    SunUBO sunUBO{};
    bool uboNeedsUpdate = true;
    bool resourcesInitialized = false;
    
    // Link to compute node for buffer sharing
    class SunParticleComputeNode* computeNode = nullptr;
    
    // Thread-safe counters
    mutable std::atomic<uint32_t> debugCounter{0};
    mutable std::atomic<uint32_t> computeCounter{0};
    mutable std::atomic<uint32_t> renderCounter{0};
};