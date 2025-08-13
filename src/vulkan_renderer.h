#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <tuple>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>

// Forward declarations for modules
class VulkanContext;
class VulkanSwapchain;
class VulkanPipeline;
class VulkanResources;
class VulkanSync;
class ComputePipeline;
class GPUEntityManager;
class MovementCommandProcessor;
class VulkanFunctionLoader;

class VulkanRenderer {
public:
    // Frame synchronization constants for smooth 60fps
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint64_t FENCE_TIMEOUT_IMMEDIATE = 0;           // Immediate check
    static constexpr uint64_t FENCE_TIMEOUT_FRAME = 16000000;       // 16ms (one frame at 60fps)
    
    VulkanRenderer();
    ~VulkanRenderer();

    bool initialize(SDL_Window* window);
    void cleanup();
    void drawFrame();
    void setEntityPosition(float x, float y, float z);
    
    enum class ShapeType {
        Triangle,
        Square
    };
    
    void updateEntities(const std::vector<std::tuple<glm::vec3, ShapeType, glm::vec4>>& entities);
    
    // Buffer capacity management
    bool validateEntityCapacity(uint32_t entityCount, const char* source = "unknown") const;
    uint32_t getMaxCpuInstances() const { return maxCpuInstances; }
    
    // Memory safety validation for testing
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
    std::unique_ptr<VulkanFunctionLoader> functionLoader;
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<VulkanResources> resources;
    std::unique_ptr<VulkanSync> sync;
    std::unique_ptr<ComputePipeline> computePipeline;
    std::unique_ptr<GPUEntityManager> gpuEntityManager;
    std::unique_ptr<MovementCommandProcessor> movementCommandProcessor;

    // Note: Function pointers removed - now using centralized VulkanFunctionLoader

    // Helper functions
    bool recreateSwapChain();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void loadDrawingFunctions();
    void updateUniformBuffer(uint32_t currentImage);
    void updateInstanceBuffer(uint32_t currentFrame);
    void dispatchCompute(VkCommandBuffer commandBuffer, float deltaTime);
    void dispatchKeyframeCompute(VkCommandBuffer commandBuffer, float futureTime, float deltaTime, uint32_t entityBatch);
    void initializeAllKeyframes();
    void transitionBufferLayout(VkCommandBuffer commandBuffer);
    bool initializeFrameFences();
    void cleanupPartialFrameFences();
    VkResult waitForFenceRobust(VkFence fence, const char* fenceName);
    
    // Entity position for rendering (backward compatibility)
    glm::vec3 entityPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    
    // Multiple entities for rendering
    std::vector<std::tuple<glm::vec3, ShapeType, glm::vec4>> renderEntities;
    
    // Buffer capacity management
    uint32_t maxCpuInstances = 0;
    bool hasBufferCapacityWarningShown = false;
    
    // GPU compute state
    float deltaTime = 0.0f;
    float totalTime = 0.0f; // Accumulated simulation time
    
    // Key-frame look-ahead system
    static constexpr uint32_t KEYFRAME_LOOKAHEAD_FRAMES = 20;
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