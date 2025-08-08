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
class VulkanFunctionLoader;

class VulkanRenderer {
public:
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
    
    // GPU entity management
    GPUEntityManager* getGPUEntityManager() { return gpuEntityManager.get(); }
    void uploadPendingGPUEntities();
    void setDeltaTime(float deltaTime) { this->deltaTime = deltaTime; }
    
    // Camera integration
    void setWorld(flecs::world* world) { this->world = world; }
    void updateAspectRatio(int windowWidth, int windowHeight);
    
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    SDL_Window* window = nullptr;
    flecs::world* world = nullptr; // Reference to ECS world for camera access
    
    static const int MAX_FRAMES_IN_FLIGHT = 2;
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

    // Note: Function pointers removed - now using centralized VulkanFunctionLoader

    // Helper functions
    bool recreateSwapChain();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void loadDrawingFunctions();
    void updateUniformBuffer(uint32_t currentImage);
    void updateInstanceBuffer(uint32_t currentFrame);
    void dispatchCompute(VkCommandBuffer commandBuffer, float deltaTime);
    void transitionBufferLayout(VkCommandBuffer commandBuffer);
    
    // Entity position for rendering (backward compatibility)
    glm::vec3 entityPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    
    // Multiple entities for rendering
    std::vector<std::tuple<glm::vec3, ShapeType, glm::vec4>> renderEntities;
    
    // GPU compute state
    float deltaTime = 0.0f;
};