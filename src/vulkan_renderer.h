#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>
#include "vulkan/core/vulkan_constants.h"
#include "vulkan/rendering/frame_graph.h"
#include "vulkan/pipelines/pipeline_system_manager.h"

// Forward declarations for modules
class VulkanContext;
class VulkanSwapchain;
class VulkanSync;
class QueueManager;
class ResourceCoordinator;
class GPUEntityManager;
class FrameGraph;
class EntityComputeNode;
class EntityGraphicsNode;
class SwapchainPresentNode;

// New modular architecture
class RenderFrameDirector;
class CommandSubmissionService;
class FrameGraphResourceRegistry;
class GPUSynchronizationService;
class PresentationSurface;
class FrameStateManager;
class ErrorRecoveryService;

class VulkanRenderer {
public:
    
    VulkanRenderer();
    ~VulkanRenderer();

    bool initialize(SDL_Window* window);
    void cleanup();
    void drawFrame();
    
    
    // GPU entity management
    GPUEntityManager* getGPUEntityManager() { return gpuEntityManager.get(); }
    void setDeltaTime(float deltaTime) { 
        this->deltaTime = deltaTime; 
        clampedDeltaTime = deltaTime;  // Update static member for global access
    }
    
    // Camera integration
    void setWorld(flecs::world* world) { this->world = world; }
    void updateAspectRatio(int windowWidth, int windowHeight);
    void setFramebufferResized(bool resized);
    
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

    // Core Vulkan modules
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanSync> sync;
    std::unique_ptr<QueueManager> queueManager;
    std::unique_ptr<ResourceCoordinator> resourceCoordinator;
    std::unique_ptr<GPUEntityManager> gpuEntityManager;
    
    // AAA Pipeline System
    std::unique_ptr<class PipelineSystemManager> pipelineSystem;
    
    // Render graph system
    std::unique_ptr<FrameGraph> frameGraph;
    
    // New modular architecture
    std::unique_ptr<RenderFrameDirector> frameDirector;
    std::unique_ptr<CommandSubmissionService> submissionService;
    std::unique_ptr<FrameGraphResourceRegistry> resourceRegistry;
    std::unique_ptr<GPUSynchronizationService> syncService;
    std::unique_ptr<PresentationSurface> presentationSurface;
    std::unique_ptr<FrameStateManager> frameStateManager;
    std::unique_ptr<ErrorRecoveryService> errorRecoveryService;


    // Helper functions
    bool initializeModularArchitecture();
    void cleanupModularArchitecture();
    void drawFrameModular();
    
    // Logging helpers
    void logFrameSuccessIfNeeded(const char* operation);
    
    // GPU compute state
    float deltaTime = 0.0f;
    float totalTime = 0.0f; // Accumulated simulation time
    
    // Static member for global access to clamped deltaTime
    static float clampedDeltaTime;
    
    // Key-frame look-ahead system
    uint32_t frameCounter = 0;
};