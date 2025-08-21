#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

#include "frame_graph_types.h"
#include "frame_graph_node_base.h"
#include "resources/resource_manager.h"
#include "compilation/frame_graph_compiler.h"
#include "execution/barrier_manager.h"

// Forward declarations
class VulkanContext;
class VulkanSync;
class QueueManager;
class GPUTimeoutDetector;
class GPUMemoryMonitor;

// Re-export resource types for backward compatibility  
using FrameGraphBuffer = FrameGraphResources::FrameGraphBuffer;
using FrameGraphImage = FrameGraphResources::FrameGraphImage;
using FrameGraphResource = FrameGraphResources::FrameGraphResource;

// Main frame graph class - now a coordinator using modular components
class FrameGraph {
public:
    FrameGraph();
    ~FrameGraph();
    
    // Initialization
    bool initialize(const VulkanContext& context, VulkanSync* sync, QueueManager* queueManager);
    void cleanup();
    void cleanupBeforeContextDestruction();
    
    // Optional monitoring integration
    void setMemoryMonitor(GPUMemoryMonitor* monitor);
    void setTimeoutDetector(GPUTimeoutDetector* detector) { timeoutDetector_ = detector; }
    
    // Resource management (delegated to ResourceManager)
    FrameGraphTypes::ResourceId createBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage);
    FrameGraphTypes::ResourceId createImage(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage);
    FrameGraphTypes::ResourceId importExternalBuffer(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkBufferUsageFlags usage);
    FrameGraphTypes::ResourceId importExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent);
    
    // Node management
    template<typename NodeType, typename... Args>
    FrameGraphTypes::NodeId addNode(Args&&... args) {
        auto node = std::make_unique<NodeType>(std::forward<Args>(args)...);
        FrameGraphTypes::NodeId id = nextNodeId_++;
        node->nodeId = id;
        nodes_[id] = std::move(node);
        return id;
    }
    
    // Get typed node reference after creation
    template<typename NodeType>
    NodeType* getNode(FrameGraphTypes::NodeId nodeId) {
        auto it = nodes_.find(nodeId);
        if (it != nodes_.end()) {
            return dynamic_cast<NodeType*>(it->second.get());
        }
        return nullptr;
    }
    
    // Graph compilation and execution
    bool compile();
    bool isCompiled() const { return compiled_; }
    void updateFrameData(float time, float deltaTime, uint32_t frameCounter, uint32_t currentFrameIndex);
    
    // Execution result to indicate which command buffers were used
    struct ExecutionResult {
        bool computeCommandBufferUsed = false;
        bool graphicsCommandBufferUsed = false;
    };
    ExecutionResult execute(uint32_t frameIndex);
    void reset(); // Clear for next frame
    void removeSwapchainResources(); // Remove swapchain images during recreation
    
    // Resource access (delegated to ResourceManager)
    VkBuffer getBuffer(FrameGraphTypes::ResourceId id) const;
    VkImage getImage(FrameGraphTypes::ResourceId id) const;
    VkImageView getImageView(FrameGraphTypes::ResourceId id) const;
    
    // Debug (delegated to ResourceManager)
    void debugPrint() const;
    
    // Performance monitoring (delegated to ResourceManager)
    void logAllocationTelemetry() const;
    
    // Resource management and recovery (delegated to ResourceManager)
    void performResourceCleanup();
    bool isMemoryPressureCritical() const;
    void evictNonCriticalResources();
    
    // Context access for nodes
    const VulkanContext* getContext() const { return context_; }

private:
    // Core state
    const VulkanContext* context_ = nullptr;
    VulkanSync* sync_ = nullptr;
    QueueManager* queueManager_ = nullptr;
    GPUTimeoutDetector* timeoutDetector_ = nullptr;
    bool initialized_ = false;
    
    // Modular components
    FrameGraphCompilation::FrameGraphCompiler compiler_;
    FrameGraphExecution::BarrierManager barrierManager_;
    FrameGraphResources::ResourceManager resourceManager_;
    
    // Node storage
    std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>> nodes_;
    FrameGraphTypes::NodeId nextNodeId_ = 1;
    
    // Compiled execution order
    std::vector<FrameGraphTypes::NodeId> executionOrder_;
    bool compiled_ = false;
    
    // Execution helpers
    std::pair<bool, bool> analyzeQueueRequirements() const;
    void beginCommandBuffers(bool useCompute, bool useGraphics, uint32_t frameIndex);
    void endCommandBuffers(bool useCompute, bool useGraphics, uint32_t frameIndex);
    void executeNodesInOrder(uint32_t frameIndex, bool& computeExecuted);
    
    // Timeout-aware execution
    bool executeWithTimeoutMonitoring(uint32_t frameIndex, bool& computeExecuted);
    void handleExecutionTimeout();
};

