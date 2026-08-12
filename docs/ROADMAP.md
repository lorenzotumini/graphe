# Roadmap

## Current State

- Native C99/raylib app with raygui controls, dark/light theme, pan/zoom, node
  dragging, multiple layouts, dynamic graph storage, and `.graphe` loading.
- DFS is the mature graph mode: discovery/finish timestamps, node colors, and
  directed/undirected edge classification are implemented.
- BFS has its own presentation with node levels and a depth gradient, but still
  needs queue/frontier visualization.
- Dijkstra supports non-negative integer edge weights, tentative distances,
  relaxation playback, settled nodes, and shortest-path-tree highlighting. It
  currently uses the first node in the selected node order as its source.
- Tree traversal can load tree `.graphe` files and show preorder, parenthesized
  inorder, and postorder output on the expression-tree prototype.
- Graph directedness lives in `Graph`; undirected graphs store one shared edge
  linked from both endpoints so traversal stays `O(V + E)`.
- Long event sentences wrap inside an adaptive two-line banner, and reciprocal
  or parallel directed edges use separate curve lanes.
- Traversal-forest layout nudges nodes away from non-tree shortcut edges while
  preserving their depth order.
- Focused C tests cover core graph, traversal, import, weighted-view conversion,
  and Dijkstra predecessor replacement behavior.

## Next Work

- Split runtime mode state and UI more explicitly:
  - DFS: discovery/finish times, node colors, and edge classification.
  - BFS: queue/frontier state, levels, and clearer BFS tree emphasis.
  - Dijkstra: source choice, tentative/settled distances, and predecessors.
  - Tree traversal: tree-specific controls, layout, and expression output.
- Add explicit source-node selection for Dijkstra and other source-based graph
  algorithms.
- Add focused tests for DFS timestamps, BFS levels, edge classifications, and
  import edge cases.
- Improve edge rendering for self-loops.
- Continue native-window UI polish: spacing, settings grouping, and
  cross-platform visual checks.
- Consider an in-app graph editor once file loading and dynamic storage feel
  stable.

## Deferred

- Optional Graphviz layout import through `dot` or `neato` output.
- Force-directed physics layout with draggable or pinned nodes.
- WebAssembly/browser build target.
