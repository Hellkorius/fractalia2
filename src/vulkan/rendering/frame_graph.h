#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <variant>
#include <chrono>
#include "../core/vulkan_raii.h"

// Forward declarations
class VulkanContext;
class VulkanSync;
class QueueManager;
class GPUTimeoutDetector;
class GPUMemoryMonitor;

namespace FrameGraphTypes {
    using ResourceId = uint32_t;
    using NodeId = uint32_t;
    
    constexpr ResourceId INVALID_RESOURCE = 0;
    constexpr NodeId INVALID_NODE = 0;
}

// Resource types that can be managed by the frame graph
struct FrameGraphBuffer {
    vulkan_raii::Buffer buffer;
    vulkan_raii::DeviceMemory memory;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    bool isExternal = false; // Managed outside frame graph
    std::string debugName;
};

struct FrameGraphImage {
    vulkan_raii::Image image;
    vulkan_raii::ImageView view;
    vulkan_raii::DeviceMemory memory;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
    VkImageUsageFlags usage = 0;
    bool isExternal = false; // Managed outside frame graph
    std::string debugName;
};

// Union type for all frame graph resources
using FrameGraphResource = std::variant<FrameGraphBuffer, FrameGraphImage>;

// Resource access patterns for dependency tracking
enum class ResourceAccess {
    Read,
    Write,
    ReadWrite
};

// Pipeline stages for synchronization
enum class PipelineStage {
    ComputeShader,
    VertexShader,
    FragmentShader,
    ColorAttachment,
    DepthAttachment,
    Transfer
};

// Resource classification for allocation strategies
enum class ResourceCriticality {
    Critical,    // Must be device local, fail fast if not possible
    Important,   // Prefer device local, allow limited fallback
    Flexible     // Accept any memory type for allocation success
};

// Resource dependency descriptor
struct ResourceDependency {
    FrameGraphTypes::ResourceId resourceId;
    ResourceAccess access;
    PipelineStage stage;
};

// Base class for frame graph render passes
class FrameGraphNode {
public:
    virtual ~FrameGraphNode() = default;
    
    // Node identification
    virtual std::string getName() const = 0;
    virtual FrameGraphTypes::NodeId getId() const { return nodeId; }
    
    // Resource dependencies
    virtual std::vector<ResourceDependency> getInputs() const = 0;
    virtual std::vector<ResourceDependency> getOutputs() const = 0;
    
    // Execution
    virtual void setup(const class FrameGraph& frameGraph) {}
    virtual void execute(VkCommandBuffer commandBuffer, const class FrameGraph& frameGraph) = 0;
    virtual void cleanup() {}
    
    // Synchronization hints
    virtual bool needsComputeQueue() const { return false; }
    virtual bool needsGraphicsQueue() const { return true; }

protected:
    FrameGraphTypes::NodeId nodeId = FrameGraphTypes::INVALID_NODE;
    friend class FrameGraph;
};

// Main frame graph class
class FrameGraph {
public:
    FrameGraph();
    ~FrameGraph();
    
    // Initialization
    bool initialize(const VulkanContext& context, VulkanSync* sync, QueueManager* queueManager);
    void cleanup();
    void cleanupBeforeContextDestruction();
    
    // Optional monitoring integration
    void setMemoryMonitor(GPUMemoryMonitor* monitor) { memoryMonitor = monitor; }
    void setTimeoutDetector(GPUTimeoutDetector* detector) { timeoutDetector = detector; }
    
    // Resource management
    FrameGraphTypes::ResourceId createBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage);
    FrameGraphTypes::ResourceId createImage(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage);
    FrameGraphTypes::ResourceId importExternalBuffer(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkBufferUsageFlags usage);
    FrameGraphTypes::ResourceId importExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent);
    
    // Node management
    template<typename NodeType, typename... Args>
    FrameGraphTypes::NodeId addNode(Args&&... args) {
        auto node = std::make_unique<NodeType>(std::forward<Args>(args)...);
        FrameGraphTypes::NodeId id = nextNodeId++;
        node->nodeId = id;
        nodes[id] = std::move(node);
        return id;
    }
    
    // Get typed node reference after creation
    template<typename NodeType>
    NodeType* getNode(FrameGraphTypes::NodeId nodeId) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            return dynamic_cast<NodeType*>(it->second.get());
        }
        return nullptr;
    }
    
    // Graph compilation and execution
    bool compile();
    bool isCompiled() const { return compiled; }
    void updateFrameData(float time, float deltaTime, uint32_t frameCounter, uint32_t currentFrameIndex);
    
    // Execution result to indicate which command buffers were used
    struct ExecutionResult {
        bool computeCommandBufferUsed = false;
        bool graphicsCommandBufferUsed = false;
    };
    ExecutionResult execute(uint32_t frameIndex);
    void reset(); // Clear for next frame
    void removeSwapchainResources(); // Remove swapchain images during recreation
    
    // Resource access
    VkBuffer getBuffer(FrameGraphTypes::ResourceId id) const;
    VkImage getImage(FrameGraphTypes::ResourceId id) const;
    VkImageView getImageView(FrameGraphTypes::ResourceId id) const;
    
    // Debug
    void debugPrint() const;
    
    // Performance monitoring
    void logAllocationTelemetry() const;
    
    // Resource management and recovery
    void performResourceCleanup();
    bool isMemoryPressureCritical() const;
    void evictNonCriticalResources();
    
    // Context access for nodes
    const VulkanContext* getContext() const { return context; }

private:
    // Core state
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    QueueManager* queueManager = nullptr;
    bool initialized = false;
    
    // Optional monitoring integration
    GPUMemoryMonitor* memoryMonitor = nullptr;
    GPUTimeoutDetector* timeoutDetector = nullptr;
    
    // Resource storage
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphResource> resources;
    std::unordered_map<std::string, FrameGraphTypes::ResourceId> resourceNameMap;
    FrameGraphTypes::ResourceId nextResourceId = 1;
    
    // Node storage
    std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>> nodes;
    FrameGraphTypes::NodeId nextNodeId = 1;
    
    // Compiled execution order
    std::vector<FrameGraphTypes::NodeId> executionOrder;
    
    // Per-node barrier tracking for optimal async execution
    struct NodeBarrierInfo {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        std::vector<VkImageMemoryBarrier> imageBarriers;
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        FrameGraphTypes::NodeId targetNodeId = 0;
        
        void clear() {
            bufferBarriers.clear();
            imageBarriers.clear();
            targetNodeId = 0;
        }
    };
    
    // Barrier batches inserted at optimal points for async execution
    std::vector<NodeBarrierInfo> barrierBatches;
    
    // Resource write tracking for O(n) barrier analysis
    struct ResourceWriteInfo {
        FrameGraphTypes::NodeId writerNode = 0;
        PipelineStage stage = PipelineStage::ComputeShader;
        ResourceAccess access = ResourceAccess::Read;
    };
    std::unordered_map<FrameGraphTypes::ResourceId, ResourceWriteInfo> resourceWriteTracking;
    
    // Resource allocation failure telemetry
    struct AllocationTelemetry {
        uint32_t totalAttempts = 0;
        uint32_t successfulCreations = 0;
        uint32_t retriedCreations = 0;
        uint32_t fallbackAllocations = 0;
        uint32_t hostMemoryFallbacks = 0;
        uint32_t criticalResourceFailures = 0;
        
        void recordAttempt() { ++totalAttempts; }
        void recordSuccess(bool wasRetried, bool wasFallback, bool wasHostMemory) {
            ++successfulCreations;
            if (wasRetried) ++retriedCreations;
            if (wasFallback) ++fallbackAllocations;
            if (wasHostMemory) ++hostMemoryFallbacks;
        }
        void recordCriticalFailure() { ++criticalResourceFailures; }
        
        // Performance impact assessment
        float getRetryRate() const { return totalAttempts ? (float)retriedCreations / totalAttempts : 0.0f; }
        float getFallbackRate() const { return totalAttempts ? (float)fallbackAllocations / totalAttempts : 0.0f; }
        float getHostMemoryRate() const { return totalAttempts ? (float)hostMemoryFallbacks / totalAttempts : 0.0f; }
    } mutable allocationTelemetry;
    
    // Resource cleanup tracking
    struct ResourceCleanupInfo {
        std::chrono::steady_clock::time_point lastAccessTime;
        uint32_t accessCount = 0;
        ResourceCriticality criticality = ResourceCriticality::Flexible;
        bool canEvict = true;
    };
    std::unordered_map<FrameGraphTypes::ResourceId, ResourceCleanupInfo> resourceCleanupInfo;
    
    bool compiled = false;
    
    // Compilation state backup for transactional compilation
    struct CompilationState {
        std::vector<FrameGraphTypes::NodeId> executionOrder;
        std::vector<NodeBarrierInfo> barrierBatches;
        std::unordered_map<FrameGraphTypes::ResourceId, ResourceWriteInfo> resourceWriteTracking;
        bool compiled = false;
        
        void clear() {
            executionOrder.clear();
            barrierBatches.clear();
            resourceWriteTracking.clear();
            compiled = false;
        }
    };
    CompilationState backupState;
    
    // Circular dependency analysis structures
    struct DependencyPath {
        std::vector<FrameGraphTypes::NodeId> nodeChain;
        std::vector<FrameGraphTypes::ResourceId> resourceChain;
    };
    
    struct CircularDependencyReport {
        std::vector<DependencyPath> cycles;
        std::vector<std::string> resolutionSuggestions;
    };
    
    // Partial compilation support
    struct PartialCompilationResult {
        std::vector<FrameGraphTypes::NodeId> validNodes;
        std::vector<FrameGraphTypes::NodeId> problematicNodes;
        std::unordered_set<FrameGraphTypes::NodeId> cycleNodes;
        bool hasValidSubgraph = false;
    };
    
    // Internal methods
    bool createVulkanBuffer(FrameGraphBuffer& buffer);
    bool createVulkanImage(FrameGraphImage& image);
    bool buildDependencyGraph();
    bool topologicalSort();
    
    // Enhanced compilation methods
    bool topologicalSortWithCycleDetection(CircularDependencyReport& report);
    CircularDependencyReport analyzeCycles(const std::unordered_map<FrameGraphTypes::NodeId, int>& inDegree);
    PartialCompilationResult attemptPartialCompilation();
    std::vector<DependencyPath> findCyclePaths(FrameGraphTypes::NodeId startNode, 
        const std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>>& adjacencyList);
    std::vector<std::string> generateResolutionSuggestions(const std::vector<DependencyPath>& cycles);
    void backupCompilationState();
    void restoreCompilationState();
    
    void analyzeBarrierRequirements();
    void createOptimalBarrierBatches();
    void insertBarrierBatch(const NodeBarrierInfo& batch, VkCommandBuffer commandBuffer);
    
    // Execution helpers
    std::pair<bool, bool> analyzeQueueRequirements() const;
    void beginCommandBuffers(bool useCompute, bool useGraphics, uint32_t frameIndex);
    void endCommandBuffers(bool useCompute, bool useGraphics, uint32_t frameIndex);
    void insertBarriersForNode(FrameGraphTypes::NodeId nodeId, VkCommandBuffer graphicsCmd, bool& computeExecuted, bool nodeNeedsGraphics);
    void executeNodesInOrder(uint32_t frameIndex, bool& computeExecuted);
    
    // Optimized barrier helpers
    void addResourceBarrier(FrameGraphTypes::ResourceId resourceId, FrameGraphTypes::NodeId targetNode,
                           PipelineStage srcStage, PipelineStage dstStage,
                           ResourceAccess srcAccess, ResourceAccess dstAccess);
    FrameGraphTypes::NodeId findNextGraphicsNode(FrameGraphTypes::NodeId fromNode) const;
    
    // Resource helpers
    FrameGraphBuffer* getBufferResource(FrameGraphTypes::ResourceId id);
    FrameGraphImage* getImageResource(FrameGraphTypes::ResourceId id);
    const FrameGraphBuffer* getBufferResource(FrameGraphTypes::ResourceId id) const;
    const FrameGraphImage* getImageResource(FrameGraphTypes::ResourceId id) const;
    
    // Memory allocation helpers
    uint32_t findAnyCompatibleMemoryType(uint32_t typeFilter) const;
    
    // Resource classification
    ResourceCriticality classifyResource(const FrameGraphBuffer& buffer) const;
    ResourceCriticality classifyResource(const FrameGraphImage& image) const;
    
    // Advanced allocation strategies
    bool tryAllocateWithStrategy(FrameGraphBuffer& buffer, ResourceCriticality criticality);
    bool tryAllocateWithStrategy(FrameGraphImage& image, ResourceCriticality criticality);
    
    // Resource cleanup and memory management
    void updateResourceAccessTracking(FrameGraphTypes::ResourceId resourceId);
    void markResourceForEviction(FrameGraphTypes::ResourceId resourceId);
    std::vector<FrameGraphTypes::ResourceId> getEvictionCandidates();
    bool attemptResourceEviction(FrameGraphTypes::ResourceId resourceId);
    
    // Timeout-aware execution
    bool executeWithTimeoutMonitoring(uint32_t frameIndex, bool& computeExecuted);
    void handleExecutionTimeout();
};

// Helper macros for common node patterns
#define DECLARE_FRAME_GRAPH_NODE(ClassName) \
    public: \
        std::string getName() const override { return #ClassName; } \
    private:

// Concrete node implementations will go in separate files
class ComputeNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(ComputeNode)
    
public:
    ComputeNode(FrameGraphTypes::ResourceId entityBuffer, FrameGraphTypes::ResourceId positionBuffer);
    
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;
    bool needsComputeQueue() const override { return true; }
    bool needsGraphicsQueue() const override { return false; }

private:
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
};

class GraphicsNode : public FrameGraphNode {
    DECLARE_FRAME_GRAPH_NODE(GraphicsNode)
    
public:
    GraphicsNode(FrameGraphTypes::ResourceId entityBuffer, FrameGraphTypes::ResourceId positionBuffer, FrameGraphTypes::ResourceId colorTarget);
    
    std::vector<ResourceDependency> getInputs() const override;
    std::vector<ResourceDependency> getOutputs() const override;
    void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) override;

private:
    FrameGraphTypes::ResourceId entityBufferId;
    FrameGraphTypes::ResourceId positionBufferId;
    FrameGraphTypes::ResourceId colorTargetId;
};