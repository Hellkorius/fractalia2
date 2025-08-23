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
class GraphicsPipelineManager;
class VulkanSwapchain;
class ResourceCoordinator;

class SunRenderingNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(SunRenderingNode)
    
public:
    SunRenderingNode(
        FrameGraphTypes::ResourceId colorTarget,
        GraphicsPipelineManager* graphicsManager,
        VulkanSwapchain* swapchain,
        ResourceCoordinator* resourceCoordinator
    );
    
    // FrameGraphNode interface
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    
    // Queue requirements
    bool needsComputeQueue() const override { return false; }
    bool needsGraphicsQueue() const override { return true; }
    
    // Update swapchain image index for current frame
    void setImageIndex(uint32_t imageIndex) { this->imageIndex = imageIndex; }
    
    // Set current frame's swapchain image resource ID
    void setCurrentSwapchainImageId(FrameGraphTypes::ResourceId currentImageId) { this->currentSwapchainImageId = currentImageId; }
    
    // Node lifecycle
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Set world reference for camera matrix access
    void setWorld(flecs::world* world) { this->world = world; }
    
    // Sun configuration
    void setSunDirection(const glm::vec3& direction) { this->sunDirection = direction; }
    void setSunIntensity(float intensity) { this->sunIntensity = intensity; }
    void setSunSize(float size) { this->sunSize = size; }
    void setSceneCenter(const glm::vec3& center, float radius) { this->sceneCenter = glm::vec4(center, radius); }

private:
    // Update sun uniform buffer
    void updateSunUBO();
    
    // Sun UBO structure
    struct SunUBO {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 sunDirection;       // xyz = direction, w = intensity
        glm::vec4 sunPosition;        // xyz = position, w = size
        glm::vec4 sceneCenter;        // xyz = center, w = radius
        float time;
        float sunGlowRadius;
        float sunCoreRadius;
        float atmosScattering;
        // God rays parameters
        float rayLength;
        float rayWidth;
        uint32_t numRays;
        float _padding;
    } sunUBO{};
    
    // Resources
    FrameGraphTypes::ResourceId colorTargetId;
    FrameGraphTypes::ResourceId currentSwapchainImageId = 0;
    FrameGraphTypes::ResourceId sunUBOId = 0; // Created internally
    
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
    
    // Sun configuration
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.3f, -0.8f, 0.5f));
    float sunIntensity = 2.0f;
    float sunSize = 1.0f;
    glm::vec4 sceneCenter = glm::vec4(0.0f, 0.0f, 0.0f, 100.0f);
    
    // God rays configuration
    float rayLength = 400.0f;
    float rayWidth = 8.0f;
    uint32_t numRays = 32; // Number of god rays
    
    bool uboNeedsUpdate = true;
    
    // Thread-safe debug counters
    mutable std::atomic<uint32_t> debugCounter{0};
    mutable std::atomic<uint32_t> renderCounter{0};
};