#ifndef GRAPHE_GRAPH_H
#define GRAPHE_GRAPH_H

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#define GRAPHE_DISTANCE_INFINITY LLONG_MAX

typedef enum NodeColor { NODE_WHITE, NODE_GRAY, NODE_BLACK } NodeColor;

typedef enum EdgeType {
    EDGE_UNCLASSIFIED,
    EDGE_TREE,
    EDGE_BACK,
    EDGE_FORWARD,
    EDGE_CROSS
} EdgeType;

typedef enum AlgorithmMode {
    ALGORITHM_DFS,
    ALGORITHM_BFS,
    ALGORITHM_DIJKSTRA,
    ALGORITHM_TREE,
    ALGORITHM_MODE_COUNT
} AlgorithmMode;

typedef enum TreeTraversalOrder {
    TREE_ORDER_PREORDER,
    TREE_ORDER_INORDER,
    TREE_ORDER_POSTORDER
} TreeTraversalOrder;

typedef enum TraceEventType {
    TRACE_EVENT_DISCOVER_NODE,
    TRACE_EVENT_EXAMINE_EDGE,
    TRACE_EVENT_CLASSIFY_EDGE,
    TRACE_EVENT_FINISH_NODE,
    TRACE_EVENT_SET_DISTANCE,
    TRACE_EVENT_RELAX_EDGE,
    TRACE_EVENT_SETTLE_NODE
} TraceEventType;

typedef struct Node {
    char *label;
    float x;
    float y;
    int discover_time;
    int finish_time;
    int level;
    long long distance;
    NodeColor color;
} Node;

typedef struct Edge {
    int from;
    int to;
    int next_from;
    int next_to;
    int next_alpha_from;
    int next_alpha_to;
    int weight;
    EdgeType type;
} Edge;

typedef struct Graph {
    bool directed;
    Node *nodes;
    int *next_alpha_node;
    int first_alpha_node;
    int *first_out;
    int *last_out;
    int *first_alpha_out;
    size_t node_count;
    size_t node_capacity;
    Edge *edges;
    size_t edge_count;
    size_t edge_capacity;
} Graph;

typedef struct TraceEvent {
    TraceEventType type;
    int node;
    int edge;
    int from;
    int to;
    EdgeType edge_type;
    int time;
    long long distance;
    long long old_distance;
    int replaced_edge;
} TraceEvent;

typedef struct Trace {
    TraceEvent *events;
    size_t count;
    size_t capacity;
} Trace;

void graph_init(Graph *graph);
void graph_free(Graph *graph);
bool graph_copy(const Graph *source, Graph *out);
int graph_add_node(Graph *graph, const char *label);
int graph_add_edge(Graph *graph, int from, int to);
int graph_add_weighted_edge(Graph *graph, int from, int to, int weight);
int graph_find_node(const Graph *graph, const char *label);
bool graph_build_view(const Graph *source, bool directed, Graph *out);
void graph_copy_node_positions(Graph *to, const Graph *from);
bool graph_structure_equals(const Graph *left, const Graph *right);
bool graph_edge_is_visible(const Graph *graph, int edge_index);
int graph_edge_neighbor(const Graph *graph, int edge_index, int node);
int graph_next_adjacent_edge(const Graph *graph, int edge_index, int node,
                             bool alphabetical);
void graph_reset_visual_state(Graph *graph);
void graph_build_sample(Graph *graph);
void graph_build_expression_tree(Graph *graph);

const char *edge_type_name(EdgeType type);
const char *node_color_name(NodeColor color);
const char *algorithm_mode_name(AlgorithmMode mode);
const char *tree_traversal_order_name(TreeTraversalOrder order);

#endif
