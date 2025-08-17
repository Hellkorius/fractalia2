#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>
#include "vulkan/vulkan_constants.h"
#include "vulkan/frame_graph.h"

// Forward declarations for modules
class VulkanContext;
class VulkanSwapchain;
class VulkanPipeline;
class VulkanSync;
class ResourceContext;
class GPUEntityManager;
class MovementCommandProcessor;
class FrameGraph;
class EntityComputeNode;
class EntityGraphicsNode;
class SwapchainPresentNode;

class VulkanRenderer {
public:
    
    VulkanRenderer();
    ~VulkanRenderer();

    bool initialize(SDL_Window* window);
    void cleanup();
    void drawFrame();
    
    
    bool validateEntityCapacity(uint32_t entityCount, const char* source = "unknown") const;
    
    // GPU entity buffer testing
    bool testBufferOverflowProtection() const;
    
    // GPU entity management
    GPUEntityManager* getGPUEntityManager() { return gpuEntityManager.get(); }
    void uploadPendingGPUEntities();
    void setDeltaTime(float deltaTime) { 
        this->deltaTime = deltaTime; 
        clampedDeltaTime = deltaTime;  // Update static member for global access
    }
    
    // Camera integration
    void setWorld(flecs::world* world) { this->world = world; }
    void updateAspectRatio(int windowWidth, int windowHeight);
    
    // Movement command processing
    class MovementCommandProcessor* getMovementCommandProcessor() { return movementCommandProcessor.get(); }
    
    // Static access to clamped deltaTime for global use
    static float getClampedDelta() { return clampedDeltaTime; }
    
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    SDL_Window* window = nullptr;
    flecs::world* world = nullptr; // Reference to ECS world for camera access
    
    // Removed duplicate MAX_FRAMES_IN_FLIGHT - using public constant
    uint32_t currentFrame = 0;
    bool framebufferResized = false;
    bool recreationInProgress = false;

    // Module instances
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<VulkanSync> sync;
    std::unique_ptr<ResourceContext> resourceContext;
    std::unique_ptr<GPUEntityManager> gpuEntityManager;
    std::unique_ptr<MovementCommandProcessor> movementCommandProcessor;
    
    // Render graph system
    std::unique_ptr<FrameGraph> frameGraph;
    std::unique_ptr<EntityComputeNode> computeNode;
    std::unique_ptr<EntityGraphicsNode> graphicsNode;
    std::unique_ptr<SwapchainPresentNode> presentNode;


    // Helper functions
    bool recreateSwapChain();
    VkResult waitForFenceRobust(VkFence fence, const char* fenceName);
    
    // Frame graph setup
    bool initializeFrameGraph();
    void cleanupFrameGraph();
    void drawFrameWithFrameGraph();
    
    
    
    // GPU compute state
    float deltaTime = 0.0f;
    float totalTime = 0.0f; // Accumulated simulation time
    
    // Static member for global access to clamped deltaTime
    static float clampedDeltaTime;
    
    // Key-frame look-ahead system
    uint32_t frameCounter = 0;
    
    // RAII fence management for frame synchronization
    class FrameFences {
    public:
        FrameFences() = default;
        ~FrameFences() { cleanup(); }
        
        // Non-copyable, movable
        FrameFences(const FrameFences&) = delete;
        FrameFences& operator=(const FrameFences&) = delete;
        FrameFences(FrameFences&&) = default;
        FrameFences& operator=(FrameFences&&) = default;
        
        bool initialize(const class VulkanContext& context);
        void cleanup();
        
        VkFence getComputeFence(uint32_t frameIndex) const { return computeFences[frameIndex]; }
        VkFence getGraphicsFence(uint32_t frameIndex) const { return graphicsFences[frameIndex]; }
        
        bool isComputeInUse(uint32_t frameIndex) const { return computeInUse[frameIndex]; }
        bool isGraphicsInUse(uint32_t frameIndex) const { return graphicsInUse[frameIndex]; }
        void setComputeInUse(uint32_t frameIndex, bool inUse) { computeInUse[frameIndex] = inUse; }
        void setGraphicsInUse(uint32_t frameIndex, bool inUse) { graphicsInUse[frameIndex] = inUse; }
        
    private:
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> computeFences{};
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> graphicsFences{};
        std::array<bool, MAX_FRAMES_IN_FLIGHT> computeInUse{};
        std::array<bool, MAX_FRAMES_IN_FLIGHT> graphicsInUse{};
        const class VulkanContext* context = nullptr;
        bool initialized = false;
    };
    
    FrameFences frameFences;
    
    // Frame graph resource IDs
    FrameGraphTypes::ResourceId entityBufferId = 0;
    FrameGraphTypes::ResourceId positionBufferId = 0;
    FrameGraphTypes::ResourceId currentPositionBufferId = 0;
    FrameGraphTypes::ResourceId targetPositionBufferId = 0;
    FrameGraphTypes::ResourceId swapchainImageId = 0;
};