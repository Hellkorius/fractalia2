#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../rendering/frame_graph.h"
#include <flecs.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <atomic>

// Forward declarations
class ComputePipelineManager;

/**
 * Sun particle compute node - handles physics simulation for sun particles
 * Runs before SunSystemNode to update particle positions, velocities, and lifecycle
 */
class SunParticleComputeNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(SunParticleComputeNode)
    
public:
    SunParticleComputeNode(
        ComputePipelineManager* computeManager
    );
    
    ~SunParticleComputeNode();
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements - compute only
    bool needsComputeQueue() const override { return true; }
    bool needsGraphicsQueue() const override { return false; }
    
    // Node lifecycle
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Configuration
    void setWorld(flecs::world* world) { this->world = world; }
    void setParticleBufferId(FrameGraphTypes::ResourceId bufferId) { this->particleBufferId = bufferId; }
    void setMaxParticles(uint32_t count) { this->maxParticles = count; }
    
    // Access particle buffer for graphics node
    FrameGraphTypes::ResourceId getParticleBufferId() const { return particleBufferId; }

private:
    // Sun particle structure - matches SunSystemNode
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
    
    // Execute compute pass to update particles
    void executeParticleCompute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph);
    
    // Update sun and particle parameters
    void updateSunUBO();
    
    // External dependencies
    ComputePipelineManager* computeManager;
    const VulkanContext* vulkanContext = nullptr;
    
    // Resources
    FrameGraphTypes::ResourceId particleBufferId = 0;  // Set externally
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    
    // Current frame state
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
    flecs::world* world = nullptr;
    
    // Sun configuration - matches SunSystemNode
    glm::vec3 sunPosition = glm::vec3(0.0f, 50.0f, 0.0f);
    glm::vec3 sunColor = glm::vec3(1.0f, 0.9f, 0.7f);
    float sunIntensity = 2.5f;
    float sunRadius = 3.0f;
    
    // Particle parameters
    uint32_t maxParticles = 64;
    float windStrength = 0.3f;
    float gravityStrength = 0.1f;
    float rayLength = 100.0f;
    float rayIntensity = 1.5f;
    
    // Internal state
    SunUBO sunUBO{};
    bool uboNeedsUpdate = true;
    bool resourcesInitialized = false;
    
    // Thread-safe counter
    mutable std::atomic<uint32_t> computeCounter{0};
};