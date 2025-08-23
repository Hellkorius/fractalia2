#pragma once

#include "../frame_graph_types.h"
#include "dependency_graph.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
class FrameGraphNode;

namespace FrameGraphCompilation {

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

// Compilation state backup for transactional compilation
struct CompilationState {
    std::vector<FrameGraphTypes::NodeId> executionOrder;
    bool compiled = false;
    
    void clear() {
        executionOrder.clear();
        compiled = false;
    }
};

class FrameGraphCompiler {
public:
    FrameGraphCompiler() = default;
    ~FrameGraphCompiler() = default;

    // Main compilation interface
    bool compile(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                 std::vector<FrameGraphTypes::NodeId>& executionOrder);

    // Enhanced compilation with cycle detection
    bool compileWithCycleDetection(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                   std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                   CircularDependencyReport& report);

    // Fallback compilation
    PartialCompilationResult attemptPartialCompilation(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);

    // State management
    void backupState(const std::vector<FrameGraphTypes::NodeId>& executionOrder, bool compiled);
    void restoreState(std::vector<FrameGraphTypes::NodeId>& executionOrder, bool& compiled);

private:
    // Core algorithms
    bool buildDependencyGraph(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);
    bool topologicalSort(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                         std::vector<FrameGraphTypes::NodeId>& executionOrder);
    bool topologicalSortWithCycleDetection(const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                            std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                            CircularDependencyReport& report);

    // Cycle analysis
    CircularDependencyReport analyzeCycles(const std::unordered_map<FrameGraphTypes::NodeId, int>& inDegree,
                                           const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes,
                                           const DependencyGraph::GraphData& graph);
    std::vector<DependencyPath> findCyclePaths(FrameGraphTypes::NodeId startNode,
                                                const std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>>& adjacencyList,
                                                const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);
    std::vector<std::string> generateResolutionSuggestions(const std::vector<DependencyPath>& cycles,
                                                            const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes);

    // State backup
    CompilationState backupState_;
};

} // namespace FrameGraphCompilation