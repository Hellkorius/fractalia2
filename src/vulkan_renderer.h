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

// Forward declarations for modules
class VulkanContext;
class VulkanSwapchain;
class VulkanPipeline;
class VulkanResources;
class VulkanSync;
class GPUEntityManager;
class MovementCommandProcessor;

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
    void setDeltaTime(float deltaTime) { this->deltaTime = deltaTime; }
    
    // Camera integration
    void setWorld(flecs::world* world) { this->world = world; }
    void updateAspectRatio(int windowWidth, int windowHeight);
    
    // Movement command processing
    class MovementCommandProcessor* getMovementCommandProcessor() { return movementCommandProcessor.get(); }
    
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    SDL_Window* window = nullptr;
    flecs::world* world = nullptr; // Reference to ECS world for camera access
    
    // Removed duplicate MAX_FRAMES_IN_FLIGHT - using public constant
    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    // Module instances
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<VulkanResources> resources;
    std::unique_ptr<VulkanSync> sync;
    std::unique_ptr<GPUEntityManager> gpuEntityManager;
    std::unique_ptr<MovementCommandProcessor> movementCommandProcessor;


    // Helper functions
    bool recreateSwapChain();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentImage);
    void transitionBufferLayout(VkCommandBuffer commandBuffer);
    bool initializeFrameFences();
    void cleanupPartialFrameFences();
    VkResult waitForFenceRobust(VkFence fence, const char* fenceName);
    
    
    
    // GPU compute state
    float deltaTime = 0.0f;
    float totalTime = 0.0f; // Accumulated simulation time
    
    // Key-frame look-ahead system
    uint32_t frameCounter = 0;
    
    // Per-frame fence management for independent compute/graphics timelines
    struct FrameData {
        VkFence computeDone = VK_NULL_HANDLE;
        VkFence graphicsDone = VK_NULL_HANDLE;
        bool computeInUse = false;
        bool graphicsInUse = false;
        bool fencesInitialized = false;
    };
    std::vector<FrameData> frameData;
};