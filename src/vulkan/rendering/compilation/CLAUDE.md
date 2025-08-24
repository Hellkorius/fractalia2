# /home/nar/projects/fractalia2/src/vulkan/rendering/compilation

## src/vulkan/rendering/compilation/ 
(Transforms frame graph nodes into optimized execution orders through dependency analysis and topological sorting)

### dependency_graph.h
**Inputs:** Frame graph nodes with input/output resource specifications.  
**Outputs:** Dependency graph data structure with resource producers mapping, adjacency lists, and in-degree counts for topological analysis.

### dependency_graph.cpp  
**Inputs:** Collection of frame graph nodes and their resource dependencies.  
**Outputs:** Built dependency graph with O(1) resource producer lookups and adjacency relationships for efficient compilation.

### frame_graph_compiler.h
**Inputs:** Frame graph nodes requiring compilation into execution order.  
**Outputs:** Interface for topological sorting, cycle detection reports, partial compilation results, and compilation state management.

### frame_graph_compiler.cpp
**Inputs:** Frame graph dependency structures and node collections.  
**Outputs:** Topologically sorted execution orders, circular dependency analysis with resolution suggestions, and robust compilation with fallback handling.