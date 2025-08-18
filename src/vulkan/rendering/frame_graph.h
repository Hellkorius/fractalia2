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

// Forward declarations
class VulkanContext;
class VulkanSync;

namespace FrameGraphTypes {
    using ResourceId = uint32_t;
    using NodeId = uint32_t;
    
    constexpr ResourceId INVALID_RESOURCE = 0;
    constexpr NodeId INVALID_NODE = 0;
}

// Resource types that can be managed by the frame graph
struct FrameGraphBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    bool isExternal = false; // Managed outside frame graph
    std::string debugName;
};

struct FrameGraphImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
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
    bool initialize(const VulkanContext& context, VulkanSync* sync);
    void cleanup();
    
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
    
    // Resource access
    VkBuffer getBuffer(FrameGraphTypes::ResourceId id) const;
    VkImage getImage(FrameGraphTypes::ResourceId id) const;
    VkImageView getImageView(FrameGraphTypes::ResourceId id) const;
    
    // Debug
    void debugPrint() const;
    
    // Context access for nodes
    const VulkanContext* getContext() const { return context; }

private:
    // Core state
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    bool initialized = false;
    
    // Resource storage
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphResource> resources;
    std::unordered_map<std::string, FrameGraphTypes::ResourceId> resourceNameMap;
    FrameGraphTypes::ResourceId nextResourceId = 1;
    
    // Node storage
    std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>> nodes;
    FrameGraphTypes::NodeId nextNodeId = 1;
    
    // Compiled execution order
    std::vector<FrameGraphTypes::NodeId> executionOrder;
    
    // Barrier information with pipeline stages
    struct BarrierInfo {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        std::vector<VkImageMemoryBarrier> imageBarriers;
        VkPipelineStageFlags srcStage = 0;
        VkPipelineStageFlags dstStage = 0;
    };
    BarrierInfo computeToGraphicsBarriers;
    
    bool compiled = false;
    
    // Internal methods
    bool createVulkanBuffer(FrameGraphBuffer& buffer);
    bool createVulkanImage(FrameGraphImage& image);
    bool buildDependencyGraph();
    bool topologicalSort();
    void insertSynchronizationBarriers();
    void insertBarrierForResource(FrameGraphTypes::ResourceId resourceId, 
                                  PipelineStage srcStage, PipelineStage dstStage,
                                  ResourceAccess srcAccess, ResourceAccess dstAccess);
    void insertBarriersIntoCommandBuffer(VkCommandBuffer commandBuffer);
    void cleanupResources();
    
    // Resource helpers
    FrameGraphBuffer* getBufferResource(FrameGraphTypes::ResourceId id);
    FrameGraphImage* getImageResource(FrameGraphTypes::ResourceId id);
    const FrameGraphBuffer* getBufferResource(FrameGraphTypes::ResourceId id) const;
    const FrameGraphImage* getImageResource(FrameGraphTypes::ResourceId id) const;
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