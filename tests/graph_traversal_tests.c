#include "graph.h"
#include "graph_io.h"
#include "layout.h"
#include "traversal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

#define CHECK(condition, message)               \
    do {                                        \
        if (!(condition)) return fail(message); \
    } while (0)

static int test_alphabetical_edge_order(void) {
    Graph graph;
    graph_init(&graph);

    int a = graph_add_node(&graph, "A");
    int b = graph_add_node(&graph, "B");
    int c = graph_add_node(&graph, "C");

    int edge_to_c = graph_add_edge(&graph, a, c);
    int edge_to_b = graph_add_edge(&graph, a, b);

    CHECK(graph.first_alpha_out[a] == edge_to_b,
          "alphabetical adjacency should start with B");
    CHECK(graph_next_adjacent_edge(&graph, edge_to_b, a, true) == edge_to_c,
          "alphabetical adjacency should put C after B");

    graph_free(&graph);
    return 0;
}

static int test_undirected_view_collapses_reciprocal_edges(void) {
    Graph source;
    graph_init(&source);

    int a = graph_add_node(&source, "A");
    int b = graph_add_node(&source, "B");
    int c = graph_add_node(&source, "C");
    source.nodes[a].x = 42.0f;
    source.nodes[a].y = 24.0f;

    graph_add_weighted_edge(&source, a, b, 7);
    graph_add_weighted_edge(&source, b, a, 3);
    graph_add_weighted_edge(&source, a, c, 5);

    Graph undirected;
    graph_init(&undirected);
    CHECK(graph_build_view(&source, false, &undirected),
          "undirected view should build");
    CHECK(!undirected.directed, "undirected view should be marked undirected");
    CHECK(undirected.edge_count == 2,
          "undirected view should collapse reciprocal directed edges");
    CHECK(source.edge_count == 3, "source graph should remain unchanged");
    CHECK(undirected.edges[0].weight == 3,
          "collapsed reciprocal edges should keep the smaller weight");
    CHECK(undirected.edges[1].weight == 5,
          "undirected view should preserve unmatched edge weights");
    CHECK(undirected.nodes[a].x == 42.0f && undirected.nodes[a].y == 24.0f,
          "graph view should preserve node positions");

    Graph directed;
    graph_init(&directed);
    CHECK(graph_build_view(&source, true, &directed), "directed view should build");
    CHECK(directed.directed, "directed view should be marked directed");
    CHECK(directed.edge_count == 3,
          "directed view should preserve reciprocal directed edges");
    CHECK(directed.edges[0].weight == 7 && directed.edges[1].weight == 3,
          "directed view should preserve edge weights");

    graph_free(&directed);
    graph_free(&undirected);
    graph_free(&source);
    return 0;
}

static int test_weighted_edge_validation(void) {
    Graph graph;
    graph_init(&graph);
    int a = graph_add_node(&graph, "A");
    int b = graph_add_node(&graph, "B");

    CHECK(graph_add_weighted_edge(&graph, a, b, -1) < 0,
          "negative edge weights should be rejected");
    CHECK(graph.edge_count == 0,
          "rejected weighted edges should not change the graph");
    int edge = graph_add_edge(&graph, a, b);
    CHECK(edge >= 0 && graph.edges[edge].weight == 1,
          "unweighted graph API should default to weight one");

    graph_free(&graph);
    return 0;
}

static int test_dijkstra_distances_and_parent_replacement(void) {
    Graph graph;
    graph_init(&graph);

    int a = graph_add_node(&graph, "A");
    int b = graph_add_node(&graph, "B");
    int c = graph_add_node(&graph, "C");
    int d = graph_add_node(&graph, "D");
    int e = graph_add_node(&graph, "E");

    int ab = graph_add_weighted_edge(&graph, a, b, 4);
    int ac = graph_add_weighted_edge(&graph, a, c, 1);
    int cb = graph_add_weighted_edge(&graph, c, b, 2);
    int bd = graph_add_weighted_edge(&graph, b, d, 1);
    int cd = graph_add_weighted_edge(&graph, c, d, 5);

    TraversalOptions options = {
        .algorithm = ALGORITHM_DIJKSTRA,
        .alphabetical = 1,
        .tree_order = TREE_ORDER_INORDER,
    };
    Trace trace;
    traversal_trace_init(&trace);
    traversal_trace_build(&graph, &options, &trace);

    CHECK(trace.count > 0, "Dijkstra should produce a trace");
    CHECK(trace.events[0].type == TRACE_EVENT_SET_DISTANCE &&
              trace.events[0].node == a && trace.events[0].distance == 0,
          "Dijkstra should initialize the first ordered node as its source");

    int settle_count = 0;
    int replaced_ab = 0;
    for (size_t i = 0; i < trace.count; i++) {
        const TraceEvent *event = &trace.events[i];
        if (event->type == TRACE_EVENT_SETTLE_NODE) settle_count++;
        if (event->type == TRACE_EVENT_RELAX_EDGE && event->edge == cb &&
            event->replaced_edge == ab && event->old_distance == 4 &&
            event->distance == 3)
            replaced_ab = 1;
    }
    CHECK(settle_count == 4,
          "Dijkstra should settle only the four source-reachable nodes");
    CHECK(replaced_ab,
          "a shorter relaxation should record the predecessor edge it replaces");

    Graph scene;
    graph_init(&scene);
    traversal_trace_apply_prefix(&graph, &trace, trace.count, &scene);

    CHECK(scene.nodes[a].distance == 0, "source distance should be zero");
    CHECK(scene.nodes[b].distance == 3, "B should have shortest distance three");
    CHECK(scene.nodes[c].distance == 1, "C should have shortest distance one");
    CHECK(scene.nodes[d].distance == 4, "D should have shortest distance four");
    CHECK(scene.nodes[e].distance == GRAPHE_DISTANCE_INFINITY,
          "unreachable nodes should remain at infinity");
    CHECK(scene.nodes[e].color == NODE_WHITE,
          "unreachable nodes should remain unsettled");

    CHECK(scene.edges[ab].type == EDGE_UNCLASSIFIED,
          "a replaced predecessor should leave the shortest-path tree");
    CHECK(scene.edges[ac].type == EDGE_TREE && scene.edges[cb].type == EDGE_TREE &&
              scene.edges[bd].type == EDGE_TREE,
          "final predecessor edges should form the shortest-path tree");
    CHECK(scene.edges[cd].type == EDGE_UNCLASSIFIED,
          "non-shortest edges should remain outside the shortest-path tree");

    graph_free(&scene);
    traversal_trace_free(&trace);
    graph_free(&graph);
    return 0;
}

static int test_undirected_dijkstra_large_distances(void) {
    Graph graph;
    graph_init(&graph);
    graph.directed = false;

    int a = graph_add_node(&graph, "A");
    int b = graph_add_node(&graph, "B");
    int c = graph_add_node(&graph, "C");
    int d = graph_add_node(&graph, "D");
    graph_add_weighted_edge(&graph, a, b, INT_MAX);
    graph_add_weighted_edge(&graph, b, c, INT_MAX);

    TraversalOptions options = {
        .algorithm = ALGORITHM_DIJKSTRA,
        .alphabetical = 1,
        .tree_order = TREE_ORDER_INORDER,
    };
    Trace trace;
    traversal_trace_init(&trace);
    traversal_trace_build(&graph, &options, &trace);

    Graph scene;
    graph_init(&scene);
    traversal_trace_apply_prefix(&graph, &trace, trace.count, &scene);

    CHECK(scene.nodes[a].distance == 0,
          "undirected Dijkstra source should have distance zero");
    CHECK(scene.nodes[b].distance == INT_MAX,
          "Dijkstra should support the largest integer edge weight");
    CHECK(scene.nodes[c].distance == 2LL * INT_MAX,
          "Dijkstra distances should not overflow at the integer edge limit");
    CHECK(scene.nodes[d].distance == GRAPHE_DISTANCE_INFINITY,
          "disconnected undirected nodes should remain unreachable");

    graph_free(&scene);
    traversal_trace_free(&trace);
    graph_free(&graph);
    return 0;
}

static float point_segment_distance_squared(float px, float py, float from_x,
                                            float from_y, float to_x, float to_y) {
    float dx = to_x - from_x;
    float dy = to_y - from_y;
    float length_squared = dx * dx + dy * dy;
    if (length_squared <= 0.0001f) {
        float point_dx = px - from_x;
        float point_dy = py - from_y;
        return point_dx * point_dx + point_dy * point_dy;
    }

    float projection = ((px - from_x) * dx + (py - from_y) * dy) / length_squared;
    if (projection < 0.0f) projection = 0.0f;
    if (projection > 1.0f) projection = 1.0f;

    float closest_x = from_x + dx * projection;
    float closest_y = from_y + dy * projection;
    float point_dx = px - closest_x;
    float point_dy = py - closest_y;
    return point_dx * point_dx + point_dy * point_dy;
}

static int test_trace_layout_avoids_shortcut_through_node(void) {
    Graph graph;
    graph_build_sample(&graph);

    TraversalOptions options = {
        .algorithm = ALGORITHM_DIJKSTRA,
        .alphabetical = 1,
        .tree_order = TREE_ORDER_INORDER,
    };
    Trace trace;
    traversal_trace_init(&trace);
    traversal_trace_build(&graph, &options, &trace);
    layout_trace_forest(&graph, &trace, 80.0f, 76.0f, 640.0f, 118.0f);

    int b = graph_find_node(&graph, "B");
    int d = graph_find_node(&graph, "D");
    int e = graph_find_node(&graph, "E");
    CHECK(b >= 0 && d >= 0 && e >= 0, "sample graph should contain B, D, and E");

    float distance_squared = point_segment_distance_squared(
        graph.nodes[e].x, graph.nodes[e].y, graph.nodes[b].x, graph.nodes[b].y,
        graph.nodes[d].x, graph.nodes[d].y);
    CHECK(distance_squared >= 40.0f * 40.0f,
          "layout should move E away from the non-tree B-D shortcut");
    CHECK(graph.nodes[b].y < graph.nodes[e].y && graph.nodes[e].y < graph.nodes[d].y,
          "shortcut deconfliction should preserve traversal depth order");

    traversal_trace_free(&trace);
    graph_free(&graph);
    return 0;
}

static int test_load_weighted_graph_file(void) {
    Graph graph;
    graph_init(&graph);
    GraphFileKind kind = GRAPH_FILE_TREE;
    char error[160];

    CHECK(graph_load_from_file("graphs/sample_weighted.graphe", &graph, error,
                               sizeof(error), &kind),
          error);
    CHECK(kind == GRAPH_FILE_GRAPH, "weighted sample should load as a graph");
    CHECK(graph.node_count == 6, "weighted sample should contain six nodes");
    CHECK(graph.edge_count == 7, "weighted sample should contain seven edges");
    CHECK(graph.edges[0].weight == 4 && graph.edges[5].weight == 1 &&
              graph.edges[6].weight == 0,
          "loader should preserve explicit weights and default omitted weights");

    graph_free(&graph);

    graph_init(&graph);
    CHECK(!graph_load_from_file("tests/fixtures/negative_weight.graphe", &graph,
                                error, sizeof(error), &kind),
          "loader should reject negative edge weights");
    CHECK(strstr(error, "non-negative integer") != NULL,
          "negative-weight error should explain the Dijkstra constraint");

    graph_free(&graph);
    return 0;
}

static int test_undirected_bfs_classifies_each_edge_once(void) {
    Graph graph;
    graph_init(&graph);
    graph.directed = false;

    int a = graph_add_node(&graph, "A");
    int b = graph_add_node(&graph, "B");
    int c = graph_add_node(&graph, "C");

    graph_add_edge(&graph, a, b);
    graph_add_edge(&graph, b, c);
    graph_add_edge(&graph, a, c);

    TraversalOptions options = {
        .algorithm = ALGORITHM_BFS,
        .alphabetical = 0,
        .tree_order = TREE_ORDER_INORDER,
    };
    Trace trace;
    traversal_trace_init(&trace);
    traversal_trace_build(&graph, &options, &trace);

    int *classifications = calloc(graph.edge_count, sizeof(*classifications));
    CHECK(classifications != NULL, "classification scratch allocation failed");
    for (size_t i = 0; i < trace.count; i++) {
        const TraceEvent *event = &trace.events[i];
        if (event->type != TRACE_EVENT_CLASSIFY_EDGE) continue;
        if (event->edge < 0 || event->edge >= (int)graph.edge_count) {
            free(classifications);
            traversal_trace_free(&trace);
            graph_free(&graph);
            return fail("BFS classification event should reference a valid edge");
        }
        classifications[event->edge]++;
    }

    for (size_t i = 0; i < graph.edge_count; i++) {
        if (classifications[i] != 1) {
            free(classifications);
            traversal_trace_free(&trace);
            graph_free(&graph);
            return fail(
                "undirected BFS should classify each physical edge exactly once");
        }
    }

    free(classifications);
    traversal_trace_free(&trace);
    graph_free(&graph);
    return 0;
}

static int test_load_tree_file_with_free_form_values(void) {
    Graph tree;
    graph_init(&tree);
    GraphFileKind kind = GRAPH_FILE_GRAPH;
    char error[160];

    CHECK(graph_load_from_file("graphs/sample_tree.graphe", &tree, error,
                               sizeof(error), &kind),
          error);
    CHECK(kind == GRAPH_FILE_TREE, "sample_tree should load as a tree file");
    CHECK(tree.directed, "tree files should be directed from parent to child");
    CHECK(tree.node_count == 7, "sample_tree should contain seven nodes");
    CHECK(tree.edge_count == 6, "sample_tree should contain six edges");
    CHECK(strcmp(tree.nodes[2].label, "total score") == 0,
          "tree node values should keep spaces");

    Trace trace;
    traversal_trace_init(&trace);
    TraversalOptions options = {
        .algorithm = ALGORITHM_TREE,
        .alphabetical = 1,
        .tree_order = TREE_ORDER_INORDER,
    };
    traversal_trace_build(&tree, &options, &trace);
    CHECK(trace.count > 0, "tree traversal should produce events");

    traversal_trace_free(&trace);
    graph_free(&tree);
    return 0;
}

static int test_load_showcase_graph_file(void) {
    Graph graph;
    graph_init(&graph);
    GraphFileKind kind = GRAPH_FILE_TREE;
    char error[160];

    CHECK(graph_load_from_file("graphs/showcase_directed.graphe", &graph, error,
                               sizeof(error), &kind),
          error);
    CHECK(kind == GRAPH_FILE_GRAPH, "showcase_directed should load as a graph file");
    CHECK(graph.directed, "showcase_directed should be directed");
    CHECK(graph.node_count == 84, "showcase_directed should contain 84 nodes");
    CHECK(graph.edge_count == 126, "showcase_directed should contain 126 edges");

    Trace trace;
    traversal_trace_init(&trace);
    TraversalOptions options = {
        .algorithm = ALGORITHM_DFS,
        .alphabetical = 1,
        .tree_order = TREE_ORDER_INORDER,
    };
    traversal_trace_build(&graph, &options, &trace);
    CHECK(trace.count > graph.edge_count,
          "showcase graph traversal should include edge and node events");

    int *depths = malloc(graph.node_count * sizeof(*depths));
    CHECK(depths != NULL, "could not allocate showcase graph test depths");
    for (size_t i = 0; i < graph.node_count; i++) depths[i] = -1;
    depths[0] = 0;

    int tree_edges = 0;
    int back_edges = 0;
    int forward_edges = 0;
    int cross_edges = 0;
    int root_tree_children = 0;
    int first_branch_tree_children = 0;
    int max_depth = 0;
    for (size_t i = 0; i < trace.count; i++) {
        if (trace.events[i].type != TRACE_EVENT_CLASSIFY_EDGE) continue;

        if (trace.events[i].edge_type == EDGE_TREE) tree_edges++;
        if (trace.events[i].edge_type == EDGE_BACK) back_edges++;
        if (trace.events[i].edge_type == EDGE_FORWARD) forward_edges++;
        if (trace.events[i].edge_type == EDGE_CROSS) cross_edges++;
        if (trace.events[i].edge_type == EDGE_TREE && trace.events[i].from == 0) {
            root_tree_children++;
        }
        if (trace.events[i].edge_type == EDGE_TREE && trace.events[i].from == 1) {
            first_branch_tree_children++;
        }
        if (trace.events[i].edge_type == EDGE_TREE && trace.events[i].from >= 0 &&
            trace.events[i].to >= 0 && depths[trace.events[i].from] >= 0) {
            depths[trace.events[i].to] = depths[trace.events[i].from] + 1;
            if (depths[trace.events[i].to] > max_depth)
                max_depth = depths[trace.events[i].to];
        }
    }
    CHECK(tree_edges == (int)graph.node_count - 1,
          "showcase graph should produce one DFS tree");
    CHECK(back_edges > 0, "showcase graph should produce back edges");
    CHECK(forward_edges > 0, "showcase graph should produce forward edges");
    CHECK(cross_edges > 0, "showcase graph should produce cross edges");
    CHECK(root_tree_children >= 4, "showcase graph DFS tree should start wide");
    CHECK(first_branch_tree_children >= 3,
          "showcase graph DFS tree should keep branching below the root");
    CHECK(max_depth >= 5, "showcase graph DFS tree should span several levels");

    free(depths);
    traversal_trace_free(&trace);
    graph_free(&graph);
    return 0;
}

int main(void) {
    if (test_alphabetical_edge_order() != 0) return 1;
    if (test_undirected_view_collapses_reciprocal_edges() != 0) return 1;
    if (test_weighted_edge_validation() != 0) return 1;
    if (test_dijkstra_distances_and_parent_replacement() != 0) return 1;
    if (test_undirected_dijkstra_large_distances() != 0) return 1;
    if (test_trace_layout_avoids_shortcut_through_node() != 0) return 1;
    if (test_undirected_bfs_classifies_each_edge_once() != 0) return 1;
    if (test_load_weighted_graph_file() != 0) return 1;
    if (test_load_tree_file_with_free_form_values() != 0) return 1;
    if (test_load_showcase_graph_file() != 0) return 1;

    return 0;
}
