#pragma once

#include "../frame_graph_types.h"
#include "../frame_graph_node_base.h"
#include <unordered_map>
#include <vector>
#include <memory>

namespace FrameGraphCompilation {

class DependencyGraph {
public:
    struct GraphData {
        std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
        std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
        std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    };

    // Build dependency graph once and reuse across all compilation methods
    static GraphData buildGraph(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);

private:
    DependencyGraph() = default;
};

} // namespace FrameGraphCompilation