# Graphe

> _γραφή_ (_graphê_): writing, drawing, a traced line.

Graphe is a small C/raylib project for visualizing graph algorithms with color-coded operations and interactive playback. DFS, BFS, Dijkstra, and tree traversal are supported.

![Graphe DFS visualizer](assets/screenshot001.png)

## Build

Requirements: CMake 3.20+ and a C99 compiler, Ninja is optional but recommended for speed.
The default CMake setup fetches raygui and can fetch raylib when no system
package is found.

Portable debug build:

```sh
cmake -S . -B build
cmake --build build --config Debug
```

Fast GCC/Ninja build:

```sh
cmake -S . -B build/gcc-release -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build build/gcc-release
```

Run tests from whichever build directory you configured:

```sh
ctest --test-dir build -C Debug --output-on-failure
```

## Controls

| Key / Input            | Action                                |
| ---------------------- | ------------------------------------- |
| `Space`                | Play / pause                          |
| `Right` / `Left`       | Step forward / backward               |
| `0` / `Enter`          | Jump to start / end                   |
| `Tab`                  | Open / close settings                 |
| `M`                    | Cycle algorithm mode                  |
| `O`                    | Cycle tree traversal order            |
| `L`                    | Cycle layout                          |
| `T`                    | Toggle dark mode                      |
| `A`                    | Toggle alphabetical order             |
| `R`                    | Reset layout                          |
| `F12`                  | Save numbered screenshot              |
| `Ctrl` `+` / `-` / `0` | Scale UI                              |
| Drag node              | Move node (switches to manual layout) |
| Drag background        | Pan canvas                            |
| Scroll over graph      | Zoom canvas                           |

## Graph Files

Load `.graphe` files via the path field in Settings. The format is line-oriented:

```text
# directed graph
directed
node A
node B
edge A B
```

Use `undirected` instead of `directed` for undirected graphs.

Graph and tree edges may include a non-negative integer weight. The weight
defaults to `1` when omitted:

```text
edge A B 7
edge B C 0
edge A C
```

Dijkstra starts from the first node in the selected alphabetical or insertion
order. Edge weights and tentative node distances are shown in Dijkstra mode;
unreachable nodes remain at `inf`. Negative weights are rejected when a graph is
loaded.

When a directed graph is switched to an undirected view, reciprocal or parallel
edges collapse into one physical edge. If their weights differ, the smaller
weight is retained.

For trees, replace `directed` with `tree` and optionally give nodes a display
value after their ID:

```text
tree
node root +
node left beta value
edge root left
```

Loading a tree file switches the app into tree traversal mode. Example files,
including `graphs/sample_weighted.graphe`, are in `graphs/`.
