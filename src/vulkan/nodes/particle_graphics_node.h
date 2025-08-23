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
class VulkanSwapchain;
class ResourceCoordinator;

class ParticleGraphicsNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(ParticleGraphicsNode)
    
public:
    ParticleGraphicsNode(
        FrameGraphTypes::ResourceId particleBuffer,
        FrameGraphTypes::ResourceId colorTarget,
        GraphicsPipelineManager* graphicsManager,
        VulkanSwapchain* swapchain,
        ResourceCoordinator* resourceCoordinator
    );
    
    ~ParticleGraphicsNode();
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements
    bool needsComputeQueue() const override { return false; }
    bool needsGraphicsQueue() const override { return true; }
    
    // Update swapchain image index for current frame
    void setImageIndex(uint32_t imageIndex) { this->imageIndex = imageIndex; }
    
    // Set current frame's swapchain image resource ID (called each frame)
    void setCurrentSwapchainImageId(FrameGraphTypes::ResourceId currentImageId) { this->currentSwapchainImageId = currentImageId; }
    
    // Node lifecycle
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Set world reference for camera matrix access
    void setWorld(flecs::world* world) { this->world = world; }
    
    // Particle configuration
    void setMaxParticles(uint32_t count) { maxParticles = count; }
    void setSunDirection(const glm::vec3& direction) { sunDirection = direction; }
    void setSceneCenter(const glm::vec3& center, float radius) { sceneCenter = glm::vec4(center, radius); }

private:
    // Update particle uniform buffer
    void updateParticleUBO();
    
    // Create descriptor pool and allocate descriptor sets
    bool createDescriptorResources(const FrameGraph& frameGraph);
    
    // Particle UBO structure (matches shader)
    struct ParticleUBO {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec4 sunDirection;      
        glm::vec4 sunPosition;       
        glm::vec4 sceneCenter;       
        float deltaTime;
        float totalTime;
        uint32_t maxParticles;
        uint32_t emissionRate;
        float particleLifetime;
        float windStrength;
        float gravityStrength;
        float sunRayLength;
    } particleUBO{};
    
    // Resources
    FrameGraphTypes::ResourceId particleBufferId;
    FrameGraphTypes::ResourceId colorTargetId;
    FrameGraphTypes::ResourceId currentSwapchainImageId = 0; // Dynamic per-frame ID
    FrameGraphTypes::ResourceId particleUBOId = 0; // Created internally
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE; // Quad vertices for particle rendering
    ResourceHandle particleUBOHandle; // Particle UBO buffer handle
    
    // Descriptor set resources
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    const VulkanContext* vulkanContext = nullptr; // For cleanup
    
    // External dependencies (not owned)
    GraphicsPipelineManager* graphicsManager;
    VulkanSwapchain* swapchain;
    ResourceCoordinator* resourceCoordinator;
    
    // Current frame state
    uint32_t imageIndex = 0;
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
    
    // ECS world reference for camera matrices
    flecs::world* world = nullptr;
    
    // Particle configuration
    uint32_t maxParticles = 10000;
    float particleLifetime = 8.0f;
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.3f, -0.8f, 0.5f));
    glm::vec4 sceneCenter = glm::vec4(0.0f, 0.0f, 0.0f, 100.0f);
    
    // Physics parameters
    float windStrength = 0.5f;
    float gravityStrength = 0.2f;
    float sunRayLength = 200.0f;
    uint32_t emissionRate = 500;
    
    bool uboNeedsUpdate = true;
    
    // Thread-safe debug counters
    mutable std::atomic<uint32_t> debugCounter{0};
    mutable std::atomic<uint32_t> renderCounter{0};
};