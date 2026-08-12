#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *text) {
    if (text == NULL) text = "";

    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy == NULL) return NULL;

    memcpy(copy, text, length);
    return copy;
}

static size_t grown_capacity(size_t current, size_t required) {
    size_t capacity = current == 0 ? 8 : current;

    while (capacity < required) capacity *= 2;
    return capacity;
}

void graph_init(Graph *graph) {
    graph->directed = true;
    graph->nodes = NULL;
    graph->next_alpha_node = NULL;
    graph->first_alpha_node = -1;
    graph->first_out = NULL;
    graph->last_out = NULL;
    graph->first_alpha_out = NULL;
    graph->node_count = 0;
    graph->node_capacity = 0;
    graph->edges = NULL;
    graph->edge_count = 0;
    graph->edge_capacity = 0;
}

/*
 * Releases every owned allocation in the graph. Node labels are owned by the
 * graph, so any copied or imported graph must be freed through this function.
 */
void graph_free(Graph *graph) {
    for (size_t i = 0; i < graph->node_count; i++) free(graph->nodes[i].label);

    free(graph->nodes);
    free(graph->next_alpha_node);
    free(graph->first_out);
    free(graph->last_out);
    free(graph->first_alpha_out);
    free(graph->edges);

    graph_init(graph);
}

/*
 * Grows the node arrays together so every node-indexed side table stays aligned
 * with Graph.nodes. Newly allocated slots are initialized as empty adjacency and
 * alphabetical links.
 */
static bool graph_reserve_nodes(Graph *graph, size_t required) {
    if (required <= graph->node_capacity) return true;

    size_t old_capacity = graph->node_capacity;
    size_t capacity = grown_capacity(graph->node_capacity, required);

    Node *nodes = realloc(graph->nodes, capacity * sizeof(*nodes));
    if (nodes == NULL) return false;
    graph->nodes = nodes;

    int *next_alpha_node =
        realloc(graph->next_alpha_node, capacity * sizeof(*next_alpha_node));
    if (next_alpha_node == NULL) return false;
    graph->next_alpha_node = next_alpha_node;

    int *first_out = realloc(graph->first_out, capacity * sizeof(*first_out));
    if (first_out == NULL) return false;
    graph->first_out = first_out;

    int *last_out = realloc(graph->last_out, capacity * sizeof(*last_out));
    if (last_out == NULL) return false;
    graph->last_out = last_out;

    int *first_alpha_out =
        realloc(graph->first_alpha_out, capacity * sizeof(*first_alpha_out));
    if (first_alpha_out == NULL) return false;
    graph->first_alpha_out = first_alpha_out;

    for (size_t i = old_capacity; i < capacity; i++) {
        graph->nodes[i].label = NULL;
        graph->next_alpha_node[i] = -1;
        graph->first_out[i] = -1;
        graph->last_out[i] = -1;
        graph->first_alpha_out[i] = -1;
    }

    graph->node_capacity = capacity;
    return true;
}

static bool graph_reserve_edges(Graph *graph, size_t required) {
    if (required <= graph->edge_capacity) return true;

    size_t capacity = grown_capacity(graph->edge_capacity, required);
    Edge *edges = realloc(graph->edges, capacity * sizeof(*edges));
    if (edges == NULL) return false;

    graph->edges = edges;
    graph->edge_capacity = capacity;
    return true;
}

/*
 * Deep-copies graph structure, labels, adjacency links, and visual state. This
 * is used when applying traces so the animated scene can be rebuilt without
 * mutating the source graph.
 */
bool graph_copy(const Graph *source, Graph *out) {
    Graph copy;
    graph_init(&copy);
    copy.directed = source->directed;

    if (!graph_reserve_nodes(&copy, source->node_count)) {
        graph_free(&copy);
        return false;
    }

    copy.node_count = source->node_count;
    copy.first_alpha_node = source->first_alpha_node;
    for (size_t i = 0; i < source->node_count; i++) {
        copy.nodes[i] = source->nodes[i];
        copy.nodes[i].label = copy_string(source->nodes[i].label);
        if (copy.nodes[i].label == NULL) {
            copy.node_count = i;
            graph_free(&copy);
            return false;
        }
        copy.next_alpha_node[i] = source->next_alpha_node[i];
        copy.first_out[i] = source->first_out[i];
        copy.last_out[i] = source->last_out[i];
        copy.first_alpha_out[i] = source->first_alpha_out[i];
    }

    if (!graph_reserve_edges(&copy, source->edge_count)) {
        graph_free(&copy);
        return false;
    }

    copy.edge_count = source->edge_count;
    if (source->edge_count > 0)
        memcpy(copy.edges, source->edges, source->edge_count * sizeof(*copy.edges));

    graph_free(out);
    *out = copy;
    return true;
}

void graph_copy_node_positions(Graph *to, const Graph *from) {
    if (to->node_count != from->node_count) return;

    for (size_t i = 0; i < to->node_count; i++) {
        to->nodes[i].x = from->nodes[i].x;
        to->nodes[i].y = from->nodes[i].y;
    }
}

/*
 * Checks only the structural identity needed to reuse an already-allocated scene
 * graph: directedness, node labels, and weighted physical edge endpoints.
 */
bool graph_structure_equals(const Graph *left, const Graph *right) {
    if (left->directed != right->directed) return false;
    if (left->node_count != right->node_count) return false;
    if (left->edge_count != right->edge_count) return false;

    for (size_t i = 0; i < left->node_count; i++) {
        if (strcmp(left->nodes[i].label, right->nodes[i].label) != 0) return false;
    }

    for (size_t i = 0; i < left->edge_count; i++) {
        if (left->edges[i].from != right->edges[i].from ||
            left->edges[i].to != right->edges[i].to ||
            left->edges[i].weight != right->edges[i].weight) {
            return false;
        }
    }

    return true;
}

int graph_find_node(const Graph *graph, const char *label) {
    for (size_t i = 0; i < graph->node_count; i++) {
        if (strcmp(graph->nodes[i].label, label) == 0) return (int)i;
    }

    return -1;
}

static int node_follows_label_order(const Graph *graph, int left_node,
                                    int right_node) {
    return strcmp(graph->nodes[left_node].label, graph->nodes[right_node].label) <=
           0;
}

/*
 * Maintains the node label ordering used by alphabetical traversal. This is kept
 * separate from insertion order so the user can switch traversal order without
 * rebuilding the graph.
 */
static void graph_link_node_by_label(Graph *graph, int node_index) {
    int current = graph->first_alpha_node;

    if (current == -1 || node_follows_label_order(graph, node_index, current)) {
        graph->next_alpha_node[node_index] = current;
        graph->first_alpha_node = node_index;
        return;
    }

    while (graph->next_alpha_node[current] != -1) {
        int next = graph->next_alpha_node[current];
        if (node_follows_label_order(graph, node_index, next)) break;
        current = next;
    }

    graph->next_alpha_node[node_index] = graph->next_alpha_node[current];
    graph->next_alpha_node[current] = node_index;
}

int graph_add_node(Graph *graph, const char *label) {
    if (!graph_reserve_nodes(graph, graph->node_count + 1)) return -1;

    int node_index = (int)graph->node_count;
    Node *node = &graph->nodes[node_index];
    node->label = copy_string(label);
    if (node->label == NULL) return -1;

    node->x = 0.0f;
    node->y = 0.0f;
    node->discover_time = -1;
    node->finish_time = -1;
    node->level = -1;
    node->distance = GRAPHE_DISTANCE_INFINITY;
    node->color = NODE_WHITE;
    graph->next_alpha_node[node_index] = -1;
    graph->first_out[node_index] = -1;
    graph->last_out[node_index] = -1;
    graph->first_alpha_out[node_index] = -1;

    graph_link_node_by_label(graph, node_index);

    graph->node_count++;
    return node_index;
}

static int edge_neighbor_for_node(const Graph *graph, const Edge *edge, int node) {
    if (edge->from == node) return edge->to;
    if (!graph->directed && edge->to == node) return edge->from;

    return -1;
}

int graph_edge_neighbor(const Graph *graph, int edge_index, int node) {
    if (edge_index < 0 || (size_t)edge_index >= graph->edge_count) return -1;

    return edge_neighbor_for_node(graph, &graph->edges[edge_index], node);
}

static int edge_next_for_node(const Graph *graph, int edge_index, int node,
                              bool alphabetical) {
    const Edge *edge = &graph->edges[edge_index];

    if (edge->from == node)
        return alphabetical ? edge->next_alpha_from : edge->next_from;

    if (!graph->directed && edge->to == node)
        return alphabetical ? edge->next_alpha_to : edge->next_to;

    return -1;
}

int graph_next_adjacent_edge(const Graph *graph, int edge_index, int node,
                             bool alphabetical) {
    if (edge_index < 0 || (size_t)edge_index >= graph->edge_count) return -1;
    return edge_next_for_node(graph, edge_index, node, alphabetical);
}

static void set_edge_next_for_node(Graph *graph, int edge_index, int node,
                                   int next_edge, bool alphabetical) {
    Edge *edge = &graph->edges[edge_index];

    if (edge->from == node) {
        if (alphabetical) {
            edge->next_alpha_from = next_edge;
        } else {
            edge->next_from = next_edge;
        }
        return;
    }

    if (!graph->directed && edge->to == node) {
        if (alphabetical) {
            edge->next_alpha_to = next_edge;
        } else {
            edge->next_to = next_edge;
        }
    }
}

static int edge_neighbor_follows_label_order(const Graph *graph, int node,
                                             int left_edge, int right_edge) {
    int left_neighbor =
        edge_neighbor_for_node(graph, &graph->edges[left_edge], node);
    int right_neighbor =
        edge_neighbor_for_node(graph, &graph->edges[right_edge], node);

    if (left_neighbor < 0 || right_neighbor < 0) return 0;

    const Node *left = &graph->nodes[left_neighbor];
    const Node *right = &graph->nodes[right_neighbor];
    return strcmp(left->label, right->label) <= 0;
}

static void graph_link_edge_by_insertion_order(Graph *graph, int node,
                                               int edge_index) {
    if (graph->first_out[node] == -1) {
        graph->first_out[node] = edge_index;
    } else {
        set_edge_next_for_node(graph, graph->last_out[node], node, edge_index,
                               false);
    }
    graph->last_out[node] = edge_index;
}

/*
 * Inserts an edge into the per-node alphabetical adjacency list. Undirected
 * edges are linked from both endpoints, but still remain one physical Edge.
 */
static void graph_link_edge_by_target_label(Graph *graph, int node, int edge_index) {
    int current = graph->first_alpha_out[node];

    if (current == -1 ||
        edge_neighbor_follows_label_order(graph, node, edge_index, current)) {
        set_edge_next_for_node(graph, edge_index, node, current, true);
        graph->first_alpha_out[node] = edge_index;
        return;
    }

    int next = edge_next_for_node(graph, current, node, true);
    while (next != -1) {
        if (edge_neighbor_follows_label_order(graph, node, edge_index, next)) break;
        current = next;
        next = edge_next_for_node(graph, current, node, true);
    }

    set_edge_next_for_node(graph, edge_index, node, next, true);
    set_edge_next_for_node(graph, current, node, edge_index, true);
}

/*
 * Adds one physical edge and links it into all adjacency views. In undirected
 * graphs this does not create a reverse edge; the same edge is reachable from
 * both endpoints.
 */
int graph_add_weighted_edge(Graph *graph, int from, int to, int weight) {
    if (from < 0 || to < 0) return -1;
    if (weight < 0) return -1;

    if ((size_t)from >= graph->node_count || (size_t)to >= graph->node_count)
        return -1;

    if (!graph_reserve_edges(graph, graph->edge_count + 1)) return -1;

    int edge_index = (int)graph->edge_count;
    Edge *edge = &graph->edges[edge_index];
    edge->from = from;
    edge->to = to;
    edge->next_from = -1;
    edge->next_to = -1;
    edge->next_alpha_from = -1;
    edge->next_alpha_to = -1;
    edge->weight = weight;
    edge->type = EDGE_UNCLASSIFIED;

    graph_link_edge_by_insertion_order(graph, from, edge_index);
    graph_link_edge_by_target_label(graph, from, edge_index);
    if (!graph->directed && to != from) {
        graph_link_edge_by_insertion_order(graph, to, edge_index);
        graph_link_edge_by_target_label(graph, to, edge_index);
    }

    graph->edge_count++;
    return edge_index;
}

int graph_add_edge(Graph *graph, int from, int to) {
    return graph_add_weighted_edge(graph, from, to, 1);
}

bool graph_edge_is_visible(const Graph *graph, int edge_index) {
    return edge_index >= 0 && (size_t)edge_index < graph->edge_count;
}

static int graph_find_undirected_pair(const Graph *graph, int from, int to) {
    for (size_t i = 0; i < graph->edge_count; i++) {
        const Edge *edge = &graph->edges[i];

        if ((edge->from == from && edge->to == to) ||
            (edge->from == to && edge->to == from)) {
            return (int)i;
        }
    }

    return -1;
}

/*
 * Builds the graph view shown by the app. Directed views preserve every source
 * edge, while undirected views collapse reciprocal pairs into one physical edge.
 * If collapsed edges have different weights, the smaller weight preserves the
 * useful shortest-path connection in the undirected interpretation.
 */
bool graph_build_view(const Graph *source, bool directed, Graph *out) {
    Graph view;
    graph_init(&view);
    view.directed = directed;

    for (size_t i = 0; i < source->node_count; i++) {
        int node_index = graph_add_node(&view, source->nodes[i].label);
        if (node_index < 0) {
            graph_free(&view);
            return false;
        }

        view.nodes[node_index].x = source->nodes[i].x;
        view.nodes[node_index].y = source->nodes[i].y;
    }

    for (size_t i = 0; i < source->edge_count; i++) {
        const Edge *edge = &source->edges[i];

        int pair =
            directed ? -1 : graph_find_undirected_pair(&view, edge->from, edge->to);
        if (pair >= 0) {
            if (edge->weight < view.edges[pair].weight)
                view.edges[pair].weight = edge->weight;
            continue;
        }

        if (graph_add_weighted_edge(&view, edge->from, edge->to, edge->weight) < 0) {
            graph_free(&view);
            return false;
        }
    }

    graph_free(out);
    *out = view;
    return true;
}

/*
 * Clears only algorithm/animation state. Structure, labels, edge endpoints, and
 * node positions are intentionally preserved between playback steps.
 */
void graph_reset_visual_state(Graph *graph) {
    for (size_t i = 0; i < graph->node_count; i++) {
        graph->nodes[i].discover_time = -1;
        graph->nodes[i].finish_time = -1;
        graph->nodes[i].level = -1;
        graph->nodes[i].distance = GRAPHE_DISTANCE_INFINITY;
        graph->nodes[i].color = NODE_WHITE;
    }

    for (size_t i = 0; i < graph->edge_count; i++)
        graph->edges[i].type = EDGE_UNCLASSIFIED;
}

void graph_build_sample(Graph *graph) {
    graph_init(graph);

    int a = graph_add_node(graph, "A");
    int b = graph_add_node(graph, "B");
    int c = graph_add_node(graph, "C");
    int d = graph_add_node(graph, "D");
    int e = graph_add_node(graph, "E");
    int f = graph_add_node(graph, "F");

    graph_add_weighted_edge(graph, a, b, 4);
    graph_add_weighted_edge(graph, a, c, 2);
    graph_add_weighted_edge(graph, b, d, 5);
    graph_add_weighted_edge(graph, b, e, 3);
    graph_add_weighted_edge(graph, c, b, 1);
    graph_add_weighted_edge(graph, c, f, 7);
    graph_add_weighted_edge(graph, d, c, 2);
    graph_add_weighted_edge(graph, e, d, 1);
    graph_add_weighted_edge(graph, f, c, 6);
}

void graph_build_expression_tree(Graph *graph) {
    graph_init(graph);

    int multiply = graph_add_node(graph, "*");
    int plus = graph_add_node(graph, "+");
    int minus = graph_add_node(graph, "-");
    int a = graph_add_node(graph, "a");
    int b = graph_add_node(graph, "b");
    int c = graph_add_node(graph, "c");
    int d = graph_add_node(graph, "d");

    graph_add_edge(graph, multiply, plus);
    graph_add_edge(graph, multiply, minus);
    graph_add_edge(graph, plus, a);
    graph_add_edge(graph, plus, b);
    graph_add_edge(graph, minus, c);
    graph_add_edge(graph, minus, d);
}

const char *edge_type_name(EdgeType type) {
    switch (type) {
    case EDGE_UNCLASSIFIED:
        return "unclassified";
    case EDGE_TREE:
        return "tree";
    case EDGE_BACK:
        return "back";
    case EDGE_FORWARD:
        return "forward";
    case EDGE_CROSS:
        return "cross";
    default:
        return "unknown";
    }
}

const char *node_color_name(NodeColor color) {
    switch (color) {
    case NODE_WHITE:
        return "white";
    case NODE_GRAY:
        return "gray";
    case NODE_BLACK:
        return "black";
    default:
        return "unknown";
    }
}

const char *algorithm_mode_name(AlgorithmMode mode) {
    switch (mode) {
    case ALGORITHM_DFS:
        return "DFS";
    case ALGORITHM_BFS:
        return "BFS";
    case ALGORITHM_DIJKSTRA:
        return "Dijkstra";
    case ALGORITHM_TREE:
        return "Tree traversal";
    default:
        return "Unknown";
    }
}

const char *tree_traversal_order_name(TreeTraversalOrder order) {
    switch (order) {
    case TREE_ORDER_PREORDER:
        return "preorder";
    case TREE_ORDER_INORDER:
        return "inorder";
    case TREE_ORDER_POSTORDER:
        return "postorder";
    default:
        return "unknown";
    }
}
