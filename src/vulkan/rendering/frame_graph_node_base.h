#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "frame_graph_types.h"
#include <string>
#include <vector>

// Forward declarations
class FrameGraph;

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
    virtual void setup(const FrameGraph& frameGraph) {}
    virtual void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) = 0;
    virtual void cleanup() {}
    
    // Synchronization hints
    virtual bool needsComputeQueue() const { return false; }
    virtual bool needsGraphicsQueue() const { return true; }

protected:
    FrameGraphTypes::NodeId nodeId = FrameGraphTypes::INVALID_NODE;
    friend class FrameGraph;
};

// Helper macros for common node patterns
#define DECLARE_FRAME_GRAPH_NODE(ClassName) \
    public: \
        std::string getName() const override { return #ClassName; } \
    private: