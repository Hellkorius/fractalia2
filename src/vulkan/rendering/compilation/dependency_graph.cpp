#include "dependency_graph.h"

namespace FrameGraphCompilation {

DependencyGraph::GraphData DependencyGraph::buildGraph(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    GraphData graph;
    
    // Build resource producer mapping for O(1) dependency lookups
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            graph.resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Initialize adjacency list and in-degrees
    for (const auto& [nodeId, node] : nodes) {
        graph.inDegree[nodeId] = 0;
        graph.adjacencyList[nodeId] = {};
    }
    
    // Build graph edges: if nodeA produces resource that nodeB consumes, nodeA -> nodeB
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = graph.resourceProducers.find(input.resourceId);
            if (producerIt != graph.resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                graph.adjacencyList[producerNodeId].push_back(nodeId);
                graph.inDegree[nodeId]++;
            }
        }
    }
    
    return graph;
}

} // namespace FrameGraphCompilation