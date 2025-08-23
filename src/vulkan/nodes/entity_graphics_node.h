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
class GPUEntityManager;
class VulkanFunctionLoader;

class EntityGraphicsNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(EntityGraphicsNode)
    
public:
    EntityGraphicsNode(
        FrameGraphTypes::ResourceId entityBuffer, 
        FrameGraphTypes::ResourceId positionBuffer,
        FrameGraphTypes::ResourceId colorTarget,
        GraphicsPipelineManager* graphicsManager,
        VulkanSwapchain* swapchain,
        ResourceCoordinator* resourceCoordinator,
        GPUEntityManager* gpuEntityManager
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
    
    // Set current frame's swapchain image resource ID (called each frame)
    void setCurrentSwapchainImageId(FrameGraphTypes::ResourceId currentImageId) { this->currentSwapchainImageId = currentImageId; }
    
    // Node lifecycle - standardized pattern
    bool initializeNode(const FrameGraph& frameGraph) override;
    void prepareFrame(uint32_t frameIndex, float time, float deltaTime) override;
    void releaseFrame(uint32_t frameIndex) override;
    
    // Set world reference for camera matrix access
    void setWorld(flecs::world* world) { this->world = world; }
    
    // Set sun direction (must match lighting system)
    void setSunDirection(const glm::vec3& direction) { 
        sunDirection = glm::normalize(direction);
        sunPosition = -sunDirection * 200.0f; // Update sun position accordingly
        uniformBufferDirty = true; // Force UBO update
    }
    
    // Force uniform buffer update on next frame (call when camera changes)
    void markUniformBufferDirty() { uniformBufferDirty = true; }

private:
    // Internal uniform buffer update
    void updateUniformBuffer();
    
    // Sun system rendering (integrated into entity render pass)
    void renderSunSystem(VkCommandBuffer commandBuffer, const VulkanFunctionLoader& vk);
    
    // Uniform buffer optimization - cache and dirty tracking
    struct CachedUBO {
        glm::mat4 view;
        glm::mat4 proj;
    } cachedUBO{};
    
    // Helper methods for camera matrix management
    CachedUBO getCameraMatrices();
    bool updateUniformBufferData(const CachedUBO& ubo);
    
    // Resources
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId colorTargetId; // Static placeholder - not used
    FrameGraphTypes::ResourceId currentSwapchainImageId = 0; // Dynamic per-frame ID
    
    // External dependencies (not owned) - validated during execution
    GraphicsPipelineManager* graphicsManager;
    VulkanSwapchain* swapchain;
    ResourceCoordinator* resourceCoordinator;
    GPUEntityManager* gpuEntityManager;
    
    // Current frame state
    uint32_t imageIndex = 0;
    float frameTime = 0.0f;
    float frameDeltaTime = 0.0f;
    uint32_t currentFrameIndex = 0;
    
    // ECS world reference for camera matrices
    flecs::world* world = nullptr;
    
    bool uniformBufferDirty = true;  // Force update on first frame
    uint32_t lastUpdatedFrameIndex = UINT32_MAX; // Track which frame index was last updated
    
    // Sun system resources
    VkBuffer sunQuadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory sunQuadMemory = VK_NULL_HANDLE;
    bool sunResourcesInitialized = false;
    
    // Sun system state - calculated from lighting direction
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.3f, -0.8f, 0.5f)); // Same as lighting
    glm::vec3 sunPosition = -sunDirection * 800.0f;        // Far away opposite to light
    glm::vec3 sunColor = glm::vec3(1.0f, 0.98f, 0.9f);     // Pale, realistic sun
    float sunIntensity = 1.0f;                              // Subdued intensity
    uint32_t particleCount = 8;
    
    // Helper methods for sun system
    bool initializeSunResources();
    void cleanupSunResources();
    
    // Thread-safe debug counters
    mutable std::atomic<uint32_t> debugCounter{0};
    mutable std::atomic<uint32_t> noEntitiesCounter{0};
    mutable std::atomic<uint32_t> drawCounter{0};
    mutable std::atomic<uint32_t> updateCounter{0};
};