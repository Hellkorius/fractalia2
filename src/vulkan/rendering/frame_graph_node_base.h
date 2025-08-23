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
    
    // Essential execution method (timing params moved here from prepareFrame)
    virtual void execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph, float time, float deltaTime) = 0;
    
    // Optional overrides for nodes that actually need them
    virtual void onFirstUse(const FrameGraph& frameGraph) {} // Replaces initializeNode - rarely needed
    virtual void cleanup() {} // Keep this as some nodes do need cleanup
    
    
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
