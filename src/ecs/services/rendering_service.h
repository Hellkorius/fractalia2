#pragma once

#include "../core/service_locator.h"
#include "../components/component.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>
#include <string>

// Hash specialization for flecs::entity to work with std::unordered_map
namespace std {
    template<>
    struct hash<flecs::entity> {
        std::size_t operator()(const flecs::entity& entity) const noexcept {
            return std::hash<uint64_t>{}(entity.id());
        }
    };
}

// Forward declarations
class VulkanRenderer;
class GPUEntityManager;
class CameraService;
class FrameGraph;

// Render priority levels
enum class RenderPriority {
    BACKGROUND = 0,
    NORMAL = 100,
    FOREGROUND = 200,
    UI = 300,
    DEBUG = 400
};

// LOD (Level of Detail) configuration
struct LODConfig {
    std::vector<float> distances = {10.0f, 50.0f, 100.0f, 500.0f};
    std::vector<float> complexityMultipliers = {1.0f, 0.75f, 0.5f, 0.25f};
    bool enabled = true;
    float lodBias = 0.0f; // Bias to adjust LOD selection
};

// Render queue entry
struct RenderQueueEntry {
    flecs::entity entity;
    Transform transform;
    Renderable renderable;
    RenderPriority priority;
    float distanceToCamera;
    int lodLevel;
    uint32_t sortKey; // For efficient sorting
    bool visible;
    
    // Generate sort key for efficient sorting
    void generateSortKey() {
        // Pack priority (8 bits) + LOD (8 bits) + distance quantized (16 bits)
        uint32_t priorityBits = static_cast<uint32_t>(priority) & 0xFF;
        uint32_t lodBits = (static_cast<uint32_t>(lodLevel) & 0xFF) << 8;
        uint32_t distanceBits = (static_cast<uint32_t>(distanceToCamera * 10.0f) & 0xFFFF) << 16;
        sortKey = priorityBits | lodBits | distanceBits;
    }
};

// Culling statistics for debugging
struct CullingStats {
    uint32_t totalEntities = 0;
    uint32_t visibleEntities = 0;
    uint32_t frustumCulled = 0;
    uint32_t occlusionCulled = 0;
    uint32_t lodCulled = 0;
    uint32_t renderQueueSize = 0;
    float cullingTimeMs = 0.0f;
    
    void reset() {
        totalEntities = visibleEntities = frustumCulled = 0;
        occlusionCulled = lodCulled = renderQueueSize = 0;
        cullingTimeMs = 0.0f;
    }
    
    float getCullingRatio() const {
        return totalEntities > 0 ? static_cast<float>(visibleEntities) / totalEntities : 0.0f;
    }
};

// Render batch for efficient GPU submission
struct RenderBatch {
    std::vector<RenderQueueEntry> entries;
    RenderPriority priority;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    uint32_t instanceCount = 0;
    
    void clear() {
        entries.clear();
        pipeline = VK_NULL_HANDLE;
        descriptorSet = VK_NULL_HANDLE;
        instanceCount = 0;
    }
};

// Render statistics
struct RenderStats {
    uint32_t totalDrawCalls = 0;
    uint32_t totalTriangles = 0;
    uint32_t totalVertices = 0;
    uint32_t totalInstances = 0;
    uint32_t batchesSubmitted = 0;
    float cpuRenderTimeMs = 0.0f;
    float gpuRenderTimeMs = 0.0f;
    
    void reset() {
        totalDrawCalls = totalTriangles = totalVertices = 0;
        totalInstances = batchesSubmitted = 0;
        cpuRenderTimeMs = gpuRenderTimeMs = 0.0f;
    }
};

// Render service - bridge between ECS and Vulkan renderer
class RenderingService {
public:
    DECLARE_SERVICE(RenderingService);
    
    RenderingService();
    ~RenderingService();
    
    // Initialization and cleanup
    bool initialize(flecs::world& world, VulkanRenderer* renderer);
    void cleanup();
    
    // Frame processing
    void processFrame(float deltaTime);
    void beginFrame();
    void endFrame();
    
    // Entity management
    void registerRenderableEntity(flecs::entity entity);
    void unregisterRenderableEntity(flecs::entity entity);
    void updateEntityData(flecs::entity entity, const Transform& transform, const Renderable& renderable);
    void markEntityDirty(flecs::entity entity);
    
    // Render queue management
    void buildRenderQueue();
    void sortRenderQueue();
    void submitRenderQueue();
    void clearRenderQueue();
    
    // Culling system
    void performCulling();
    void setFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
    void setOcclusionCullingEnabled(bool enabled) { occlusionCullingEnabled = enabled; }
    bool isFrustumCullingEnabled() const { return frustumCullingEnabled; }
    bool isOcclusionCullingEnabled() const { return occlusionCullingEnabled; }
    
    // LOD (Level of Detail) management
    void setLODConfig(const LODConfig& config) { lodConfig = config; }
    const LODConfig& getLODConfig() const { return lodConfig; }
    int calculateLOD(const glm::vec3& entityPosition, const glm::vec3& cameraPosition) const;
    void setLODEnabled(bool enabled) { lodConfig.enabled = enabled; }
    bool isLODEnabled() const { return lodConfig.enabled; }
    
    // Render batching
    void createRenderBatches();
    void submitBatch(const RenderBatch& batch);
    void setBatchingEnabled(bool enabled) { batchingEnabled = enabled; }
    bool isBatchingEnabled() const { return batchingEnabled; }
    
    // Statistics and monitoring
    const CullingStats& getCullingStats() const { return cullingStats; }
    const RenderStats& getRenderStats() const { return renderStats; }
    void resetStats();
    void printStats() const;
    
    // Configuration
    void setMaxRenderableEntities(uint32_t maxEntities) { maxRenderableEntities = maxEntities; }
    uint32_t getMaxRenderableEntities() const { return maxRenderableEntities; }
    void setRenderDistance(float distance) { maxRenderDistance = distance; }
    float getRenderDistance() const { return maxRenderDistance; }
    
    // Debug and profiling
    void setDebugVisualization(bool enabled) { debugVisualization = enabled; }
    bool isDebugVisualizationEnabled() const { return debugVisualization; }
    void drawDebugInfo();
    void setWireframeMode(bool enabled) { wireframeMode = enabled; }
    bool isWireframeModeEnabled() const { return wireframeMode; }
    
    // Render callbacks for custom processing
    using PreRenderCallback = std::function<void(const std::vector<RenderQueueEntry>&)>;
    using PostRenderCallback = std::function<void(const RenderStats&)>;
    void setPreRenderCallback(PreRenderCallback callback) { preRenderCallback = callback; }
    void setPostRenderCallback(PostRenderCallback callback) { postRenderCallback = callback; }
    
    // Multi-threaded rendering support
    void setMultithreadingEnabled(bool enabled) { multithreadingEnabled = enabled; }
    bool isMultithreadingEnabled() const { return multithreadingEnabled; }
    void processRenderingMT(); // Multi-threaded version
    
    // Integration with existing systems
    void syncWithGPUEntityManager();
    void updateFromECS();
    void prepareGPUData();
    
    // Viewport and camera integration
    void setActiveViewport(const std::string& viewportName) { activeViewportName = viewportName; }
    const std::string& getActiveViewport() const { return activeViewportName; }
    void renderViewport(const std::string& viewportName);
    void renderAllViewports();
    
    // Service access (for convenience namespaces)
    CameraService* getCameraService() const { return cameraService; }

private:
    // Core data
    flecs::world* world = nullptr;
    VulkanRenderer* renderer = nullptr;
    GPUEntityManager* gpuEntityManager = nullptr;
    bool initialized = false;
    
    // Service dependencies (cached references)
    CameraService* cameraService = nullptr;
    
    // Render queue
    std::vector<RenderQueueEntry> renderQueue;
    std::vector<RenderBatch> renderBatches;
    std::unordered_map<flecs::entity, uint32_t> entityToQueueIndex;
    
    // Culling and LOD
    LODConfig lodConfig;
    bool frustumCullingEnabled = true;
    bool occlusionCullingEnabled = false;
    bool batchingEnabled = true;
    float maxRenderDistance = 1000.0f;
    
    // Configuration
    uint32_t maxRenderableEntities = 100000;
    bool debugVisualization = false;
    bool wireframeMode = false;
    bool multithreadingEnabled = false;
    
    // Statistics
    CullingStats cullingStats;
    RenderStats renderStats;
    
    // Timing
    float deltaTime = 0.0f;
    uint64_t frameNumber = 0;
    
    // Callbacks
    PreRenderCallback preRenderCallback;
    PostRenderCallback postRenderCallback;
    
    // Viewport management
    std::string activeViewportName = "default";
    
    // Internal methods
    void collectRenderableEntities();
    bool isEntityVisible(const RenderQueueEntry& entry) const;
    bool performFrustumCulling(const Transform& transform, const Bounds& bounds) const;
    bool performOcclusionCulling(const Transform& transform, const Bounds& bounds) const;
    void updateCullingStats();
    void updateRenderStats();
    
    // Render queue helpers
    RenderQueueEntry createQueueEntry(flecs::entity entity, const Transform& transform, const Renderable& renderable);
    void sortQueueByPriority();
    void sortQueueByDistance();
    void sortQueueByState();
    
    // Batching helpers
    bool canBatchTogether(const RenderQueueEntry& a, const RenderQueueEntry& b) const;
    void optimizeBatches();
    
    // ECS integration
    void queryRenderableEntities();
    void processEntityUpdates();
    void handleComponentChanges();
    
    // GPU synchronization
    void waitForGPUIdle();
    void flushPendingOperations();
    
    // Debug helpers
    void renderDebugBounds();
    void renderDebugLOD();
    void renderDebugCulling();
};

// Convenience functions for global access
namespace Rendering {
    RenderingService& getService();
    
    // Quick registration
    void registerEntity(flecs::entity entity);
    void unregisterEntity(flecs::entity entity);
    
    // Quick queries
    bool isVisible(flecs::entity entity);
    int getLODLevel(flecs::entity entity);
    float getDistanceToCamera(flecs::entity entity);
    
    // Quick configuration
    void setLODEnabled(bool enabled);
    void setFrustumCullingEnabled(bool enabled);
    void setDebugMode(bool enabled);
    
    // Quick statistics
    uint32_t getVisibleEntityCount();
    float getCullingRatio();
    uint32_t getDrawCallCount();
    float getRenderTime();
}