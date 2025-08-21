#include "frame_graph_compiler.h"
#include "../frame_graph_node_base.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include <cassert>
#include <functional>

namespace FrameGraphCompilation {

bool FrameGraphCompiler::compile(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                 std::vector<FrameGraphTypes::NodeId>& executionOrder) {
    if (!buildDependencyGraph(nodes)) {
        return false;
    }
    
    return topologicalSort(nodes, executionOrder);
}

bool FrameGraphCompiler::compileWithCycleDetection(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                                    std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                                    CircularDependencyReport& report) {
    if (!buildDependencyGraph(nodes)) {
        return false;
    }
    
    return topologicalSortWithCycleDetection(nodes, executionOrder, report);
}

PartialCompilationResult FrameGraphCompiler::attemptPartialCompilation(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    PartialCompilationResult result;
    
    // Build resource producer mapping
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Build adjacency list
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
    std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    
    for (const auto& [nodeId, node] : nodes) {
        inDegree[nodeId] = 0;
        adjacencyList[nodeId] = {};
    }
    
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = resourceProducers.find(input.resourceId);
            if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                adjacencyList[producerNodeId].push_back(nodeId);
                inDegree[nodeId]++;
            }
        }
    }
    
    // Identify cycle nodes
    std::queue<FrameGraphTypes::NodeId> zeroInDegreeQueue;
    std::vector<FrameGraphTypes::NodeId> tempExecutionOrder;
    
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(nodeId);
        }
    }
    
    // Process acyclic portion
    auto tempInDegree = inDegree;
    while (!zeroInDegreeQueue.empty()) {
        FrameGraphTypes::NodeId currentNode = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        
        tempExecutionOrder.push_back(currentNode);
        result.validNodes.push_back(currentNode);
        
        for (FrameGraphTypes::NodeId dependentNode : adjacencyList[currentNode]) {
            tempInDegree[dependentNode]--;
            if (tempInDegree[dependentNode] == 0) {
                zeroInDegreeQueue.push(dependentNode);
            }
        }
    }
    
    // Identify problematic nodes
    for (const auto& [nodeId, degree] : tempInDegree) {
        if (degree > 0) {
            result.problematicNodes.push_back(nodeId);
            result.cycleNodes.insert(nodeId);
        }
    }
    
    result.hasValidSubgraph = !result.validNodes.empty();
    
    return result;
}

void FrameGraphCompiler::backupState(const std::vector<FrameGraphTypes::NodeId>& executionOrder, bool compiled) {
    backupState_.executionOrder = executionOrder;
    backupState_.compiled = compiled;
}

void FrameGraphCompiler::restoreState(std::vector<FrameGraphTypes::NodeId>& executionOrder, bool& compiled) {
    executionOrder = backupState_.executionOrder;
    compiled = backupState_.compiled;
}

bool FrameGraphCompiler::buildDependencyGraph(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    // For now, implement a simple dependency system
    // In a full implementation, this would build a proper dependency graph
    // based on resource reads/writes between nodes
    return true;
}

bool FrameGraphCompiler::topologicalSort(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                          std::vector<FrameGraphTypes::NodeId>& executionOrder) {
    // Efficient O(V + E) topological sort using Kahn's algorithm
    executionOrder.clear();
    
    // Build resource producer mapping for O(1) dependency lookups
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Build adjacency list and calculate in-degrees
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
    std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    
    // Initialize in-degrees to 0
    for (const auto& [nodeId, node] : nodes) {
        inDegree[nodeId] = 0;
        adjacencyList[nodeId] = {};
    }
    
    // Build graph edges: if nodeA produces resource that nodeB consumes, nodeA -> nodeB
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = resourceProducers.find(input.resourceId);
            if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                adjacencyList[producerNodeId].push_back(nodeId);
                inDegree[nodeId]++;
            }
        }
    }
    
    // Kahn's algorithm: start with nodes that have no dependencies
    std::queue<FrameGraphTypes::NodeId> zeroInDegreeQueue;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(nodeId);
        }
    }
    
    size_t processedNodes = 0;
    while (!zeroInDegreeQueue.empty()) {
        FrameGraphTypes::NodeId currentNode = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        
        executionOrder.push_back(currentNode);
        processedNodes++;
        
        // Reduce in-degree for all dependent nodes
        for (FrameGraphTypes::NodeId dependentNode : adjacencyList[currentNode]) {
            inDegree[dependentNode]--;
            if (inDegree[dependentNode] == 0) {
                zeroInDegreeQueue.push(dependentNode);
            }
        }
    }
    
    // Check for circular dependencies
    if (processedNodes != nodes.size()) {
        std::cerr << "FrameGraphCompiler: Circular dependency detected. Processed " << processedNodes 
                  << " nodes out of " << nodes.size() << std::endl;
        
        // Report nodes involved in cycles (for debugging)
        for (const auto& [nodeId, degree] : inDegree) {
            if (degree > 0) {
                auto nodeIt = nodes.find(nodeId);
                if (nodeIt != nodes.end()) {
                    std::cerr << "FrameGraphCompiler: Node in cycle: " << nodeIt->second->getName() 
                              << " (ID: " << nodeId << ", remaining in-degree: " << degree << ")" << std::endl;
                }
            }
        }
        return false;
    }
    
    return true;
}

bool FrameGraphCompiler::topologicalSortWithCycleDetection(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                                            std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                                            CircularDependencyReport& report) {
    // Efficient O(V + E) topological sort using Kahn's algorithm with enhanced cycle detection
    executionOrder.clear();
    
    // Build resource producer mapping for O(1) dependency lookups
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Build adjacency list and calculate in-degrees
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
    std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    
    // Initialize in-degrees to 0
    for (const auto& [nodeId, node] : nodes) {
        inDegree[nodeId] = 0;
        adjacencyList[nodeId] = {};
    }
    
    // Build graph edges: if nodeA produces resource that nodeB consumes, nodeA -> nodeB
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = resourceProducers.find(input.resourceId);
            if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                adjacencyList[producerNodeId].push_back(nodeId);
                inDegree[nodeId]++;
            }
        }
    }
    
    // Kahn's algorithm: start with nodes that have no dependencies
    std::queue<FrameGraphTypes::NodeId> zeroInDegreeQueue;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(nodeId);
        }
    }
    
    size_t processedNodes = 0;
    while (!zeroInDegreeQueue.empty()) {
        FrameGraphTypes::NodeId currentNode = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        
        executionOrder.push_back(currentNode);
        processedNodes++;
        
        // Reduce in-degree for all dependent nodes
        for (FrameGraphTypes::NodeId dependentNode : adjacencyList[currentNode]) {
            inDegree[dependentNode]--;
            if (inDegree[dependentNode] == 0) {
                zeroInDegreeQueue.push(dependentNode);
            }
        }
    }
    
    // Check for circular dependencies
    if (processedNodes != nodes.size()) {
        // Enhanced cycle analysis
        report = analyzeCycles(inDegree, nodes);
        return false;
    }
    
    return true;
}

CircularDependencyReport FrameGraphCompiler::analyzeCycles(const std::unordered_map<FrameGraphTypes::NodeId, int>& inDegree,
                                                            const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    CircularDependencyReport report;
    
    // Find nodes involved in cycles (those with remaining in-degree > 0)
    std::unordered_set<FrameGraphTypes::NodeId> cycleNodes;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree > 0) {
            cycleNodes.insert(nodeId);
        }
    }
    
    // Build adjacency list for cycle nodes only
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> cycleAdjacencyList;
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    
    // Build resource producers map
    for (const auto& [nodeId, node] : nodes) {
        if (cycleNodes.find(nodeId) != cycleNodes.end()) {
            auto outputs = node->getOutputs();
            for (const auto& output : outputs) {
                resourceProducers[output.resourceId] = nodeId;
            }
        }
    }
    
    // Build cycle adjacency list with resource tracking
    for (const auto& nodeId : cycleNodes) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt != nodes.end()) {
            auto inputs = nodeIt->second->getInputs();
            for (const auto& input : inputs) {
                auto producerIt = resourceProducers.find(input.resourceId);
                if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                    FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                    if (cycleNodes.find(producerNodeId) != cycleNodes.end()) {
                        cycleAdjacencyList[producerNodeId].push_back(nodeId);
                    }
                }
            }
        }
    }
    
    // Find actual cycle paths using DFS
    std::unordered_set<FrameGraphTypes::NodeId> visited;
    for (const auto& startNode : cycleNodes) {
        if (visited.find(startNode) == visited.end()) {
            std::vector<DependencyPath> cyclePaths = findCyclePaths(startNode, cycleAdjacencyList, nodes);
            report.cycles.insert(report.cycles.end(), cyclePaths.begin(), cyclePaths.end());
            
            // Mark all nodes in found cycles as visited
            for (const auto& path : cyclePaths) {
                for (const auto& nodeId : path.nodeChain) {
                    visited.insert(nodeId);
                }
            }
        }
    }
    
    // Generate resolution suggestions
    report.resolutionSuggestions = generateResolutionSuggestions(report.cycles, nodes);
    
    return report;
}

std::vector<DependencyPath> FrameGraphCompiler::findCyclePaths(FrameGraphTypes::NodeId startNode,
                                                                const std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>>& adjacencyList,
                                                                const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    
    std::vector<DependencyPath> cycles;
    std::vector<FrameGraphTypes::NodeId> path;
    std::unordered_set<FrameGraphTypes::NodeId> inPath;
    
    std::function<void(FrameGraphTypes::NodeId)> dfs = [&](FrameGraphTypes::NodeId node) {
        if (inPath.find(node) != inPath.end()) {
            // Found a cycle - extract the cycle from the path
            DependencyPath cycle;
            bool inCycle = false;
            for (size_t i = 0; i < path.size(); i++) {
                if (path[i] == node) {
                    inCycle = true;
                }
                if (inCycle) {
                    cycle.nodeChain.push_back(path[i]);
                    
                    // Find the resource that creates the dependency
                    if (i + 1 < path.size()) {
                        FrameGraphTypes::NodeId nextNode = path[i + 1];
                        auto nextNodeIt = nodes.find(nextNode);
                        if (nextNodeIt != nodes.end()) {
                            auto inputs = nextNodeIt->second->getInputs();
                            for (const auto& input : inputs) {
                                // Find which resource from current node is consumed by next node
                                auto currentNodeIt = nodes.find(path[i]);
                                if (currentNodeIt != nodes.end()) {
                                    auto outputs = currentNodeIt->second->getOutputs();
                                    for (const auto& output : outputs) {
                                        if (output.resourceId == input.resourceId) {
                                            cycle.resourceChain.push_back(input.resourceId);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            cycle.nodeChain.push_back(node); // Close the cycle
            cycles.push_back(cycle);
            return;
        }
        
        path.push_back(node);
        inPath.insert(node);
        
        auto adjIt = adjacencyList.find(node);
        if (adjIt != adjacencyList.end()) {
            for (FrameGraphTypes::NodeId neighbor : adjIt->second) {
                dfs(neighbor);
            }
        }
        
        path.pop_back();
        inPath.erase(node);
    };
    
    dfs(startNode);
    return cycles;
}

std::vector<std::string> FrameGraphCompiler::generateResolutionSuggestions(const std::vector<DependencyPath>& cycles,
                                                                            const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    std::vector<std::string> suggestions;
    
    if (cycles.empty()) {
        return suggestions;
    }
    
    suggestions.push_back("Consider these resolution strategies:");
    
    // Analyze each cycle for specific suggestions
    for (size_t i = 0; i < cycles.size(); i++) {
        const auto& cycle = cycles[i];
        
        suggestions.push_back("Cycle " + std::to_string(i + 1) + " resolution options:");
        
        // Suggest breaking the cycle by removing dependencies
        if (cycle.nodeChain.size() >= 2) {
            auto node1It = nodes.find(cycle.nodeChain[0]);
            auto node2It = nodes.find(cycle.nodeChain[1]);
            if (node1It != nodes.end() && node2It != nodes.end()) {
                suggestions.push_back("  • Remove dependency between " + node1It->second->getName() + 
                                    " and " + node2It->second->getName());
            }
        }
        
        // Suggest intermediate resources
        suggestions.push_back("  • Introduce intermediate buffer/texture to break direct dependency");
        
        // Suggest reordering operations
        suggestions.push_back("  • Consider if operations can be reordered or merged");
        
        // Suggest alternative data flow
        suggestions.push_back("  • Use separate render targets or double buffering");
    }
    
    suggestions.push_back("General strategies:");
    suggestions.push_back("  • Split complex nodes into smaller, independent operations");
    suggestions.push_back("  • Use temporal separation (multi-pass rendering)");
    suggestions.push_back("  • Consider if read-after-write can be converted to write-after-read");
    
    return suggestions;
}

} // namespace FrameGraphCompilation