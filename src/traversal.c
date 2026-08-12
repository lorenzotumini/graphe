#include "traversal.h"

#include <stdlib.h>
#include <string.h>

typedef struct TraversalState {
    NodeColor *colors;
    int *discover_times;
    int *levels;
    int *parent_edges;
    int *edge_classified;
    int time;
} TraversalState;

typedef struct DijkstraHeap {
    int *nodes;
    int *positions;
    const long long *distances;
    const int *ranks;
    size_t count;
    size_t capacity;
} DijkstraHeap;

static const TraversalOptions default_options = {
    .algorithm = ALGORITHM_DFS,
    .alphabetical = 1,
    .tree_order = TREE_ORDER_INORDER,
};

void traversal_trace_init(Trace *trace) {
    trace->events = NULL;
    trace->count = 0;
    trace->capacity = 0;
}

void traversal_trace_free(Trace *trace) {
    free(trace->events);
    traversal_trace_init(trace);
}

static bool trace_reserve_events(Trace *trace, size_t required) {
    if (required <= trace->capacity) return true;

    size_t capacity = trace->capacity == 0 ? 64 : trace->capacity;
    while (capacity < required) capacity *= 2;

    TraceEvent *events = realloc(trace->events, capacity * sizeof(*events));
    if (events == NULL) return false;

    trace->events = events;
    trace->capacity = capacity;
    return true;
}

static void push_event(Trace *trace, TraceEvent event) {
    if (!trace_reserve_events(trace, trace->count + 1)) return;

    trace->events[trace->count] = event;
    trace->count++;
}

static TraceEvent make_node_event(TraceEventType type, int node, int time) {
    TraceEvent event;

    event.type = type;
    event.node = node;
    event.edge = -1;
    event.from = -1;
    event.to = -1;
    event.edge_type = EDGE_UNCLASSIFIED;
    event.time = time;
    event.distance = GRAPHE_DISTANCE_INFINITY;
    event.old_distance = GRAPHE_DISTANCE_INFINITY;
    event.replaced_edge = -1;

    return event;
}

static TraceEvent make_edge_event(TraceEventType type, int edge, int from, int to,
                                  EdgeType edge_type) {
    TraceEvent event;

    event.type = type;
    event.node = -1;
    event.edge = edge;
    event.from = from;
    event.to = to;
    event.edge_type = edge_type;
    event.time = -1;
    event.distance = GRAPHE_DISTANCE_INFINITY;
    event.old_distance = GRAPHE_DISTANCE_INFINITY;
    event.replaced_edge = -1;

    return event;
}

static TraceEvent make_edge_event_with_time(TraceEventType type, int edge, int from,
                                            int to, EdgeType edge_type, int time) {
    TraceEvent event = make_edge_event(type, edge, from, to, edge_type);
    event.time = time;
    return event;
}

static TraceEvent make_distance_event(TraceEventType type, int node,
                                      long long distance, long long old_distance) {
    TraceEvent event = make_node_event(type, node, -1);
    event.distance = distance;
    event.old_distance = old_distance;
    return event;
}

static TraceEvent make_relax_event(int edge, int from, int to, long long distance,
                                   long long old_distance, int replaced_edge) {
    TraceEvent event =
        make_edge_event(TRACE_EVENT_RELAX_EDGE, edge, from, to, EDGE_TREE);
    event.node = to;
    event.distance = distance;
    event.old_distance = old_distance;
    event.replaced_edge = replaced_edge;
    return event;
}

static void traversal_state_free(TraversalState *state) {
    free(state->colors);
    free(state->discover_times);
    free(state->levels);
    free(state->parent_edges);
    free(state->edge_classified);
    memset(state, 0, sizeof(*state));
}

static bool traversal_state_init(const Graph *graph, TraversalState *state) {
    memset(state, 0, sizeof(*state));
    state->time = 0;

    if (graph->node_count > 0) {
        state->colors = malloc(graph->node_count * sizeof(*state->colors));
        state->discover_times =
            malloc(graph->node_count * sizeof(*state->discover_times));
        state->levels = malloc(graph->node_count * sizeof(*state->levels));
        state->parent_edges =
            malloc(graph->node_count * sizeof(*state->parent_edges));
        if (state->colors == NULL || state->discover_times == NULL ||
            state->levels == NULL || state->parent_edges == NULL) {
            traversal_state_free(state);
            return false;
        }
    }

    size_t edge_slots = graph->edge_count == 0 ? 1 : graph->edge_count;
    state->edge_classified = calloc(edge_slots, sizeof(*state->edge_classified));
    if (state->edge_classified == NULL) {
        traversal_state_free(state);
        return false;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        state->colors[i] = NODE_WHITE;
        state->discover_times[i] = -1;
        state->levels[i] = -1;
        state->parent_edges[i] = -1;
    }

    return true;
}

static int first_out_edge(const Graph *graph, int node,
                          const TraversalOptions *options) {
    if (options->alphabetical) return graph->first_alpha_out[node];
    return graph->first_out[node];
}

static int next_out_edge(const Graph *graph, int edge_id, int node,
                         const TraversalOptions *options) {
    return graph_next_adjacent_edge(graph, edge_id, node, options->alphabetical);
}

static int next_out_edge_in_insertion_order(const Graph *graph, int edge_id,
                                            int node) {
    return graph_next_adjacent_edge(graph, edge_id, node, false);
}

static int first_out_edge_in_insertion_order(const Graph *graph, int node) {
    return graph->first_out[node];
}

static int first_root_node(const Graph *graph, const TraversalOptions *options) {
    if (graph->node_count == 0) return -1;
    if (options->alphabetical) return graph->first_alpha_node;
    return 0;
}

static int next_root_node(const Graph *graph, int node,
                          const TraversalOptions *options) {
    if (options->alphabetical) return graph->next_alpha_node[node];

    node++;
    if ((size_t)node >= graph->node_count) return -1;
    return node;
}

static void discover_node(TraversalState *state, Trace *trace, int node) {
    state->colors[node] = NODE_GRAY;
    state->time++;
    state->discover_times[node] = state->time;
    push_event(trace, make_node_event(TRACE_EVENT_DISCOVER_NODE, node, state->time));
}

static void discover_node_at_level(TraversalState *state, Trace *trace, int node,
                                   int level) {
    state->colors[node] = NODE_GRAY;
    state->levels[node] = level;
    push_event(trace, make_node_event(TRACE_EVENT_DISCOVER_NODE, node, level));
}

static void finish_node(TraversalState *state, Trace *trace, int node) {
    state->colors[node] = NODE_BLACK;
    state->time++;
    push_event(trace, make_node_event(TRACE_EVENT_FINISH_NODE, node, state->time));
}

/*
 * DFS edge classification needs discovery times and current colors. Gray targets
 * are ancestors, white targets become tree edges, and finished descendants vs
 * unrelated finished nodes split forward from cross edges.
 */
static EdgeType classify_dfs_seen_edge(const TraversalState *state, int from,
                                       int to) {
    if (state->colors[to] == NODE_GRAY) return EDGE_BACK;

    if (state->discover_times[from] < state->discover_times[to]) return EDGE_FORWARD;

    return EDGE_CROSS;
}

static bool is_reverse_parent_edge(const Graph *graph, int edge_id,
                                   int parent_edge) {
    if (graph->directed || parent_edge < 0) return false;

    return parent_edge == edge_id;
}

/*
 * Emits the DFS teaching trace: discover node, examine edge, classify edge, then
 * finish node. Undirected graphs classify each physical edge once even though it
 * appears in both endpoint adjacency lists.
 */
static void dfs_visit_node(const Graph *graph, int node, int parent_edge,
                           const TraversalOptions *options, TraversalState *state,
                           Trace *trace) {
    discover_node(state, trace, node);

    for (int edge_id = first_out_edge(graph, node, options); edge_id != -1;
         edge_id = next_out_edge(graph, edge_id, node, options)) {
        int neighbor = graph_edge_neighbor(graph, edge_id, node);

        if (neighbor < 0) continue;
        if (is_reverse_parent_edge(graph, edge_id, parent_edge)) continue;
        if (!graph->directed && state->edge_classified[edge_id]) continue;
        if (!graph->directed && state->colors[neighbor] == NODE_BLACK) continue;

        push_event(trace, make_edge_event(TRACE_EVENT_EXAMINE_EDGE, edge_id, node,
                                          neighbor, EDGE_UNCLASSIFIED));

        if (state->colors[neighbor] == NODE_WHITE) {
            state->parent_edges[neighbor] = edge_id;
            state->edge_classified[edge_id] = 1;
            push_event(trace, make_edge_event(TRACE_EVENT_CLASSIFY_EDGE, edge_id,
                                              node, neighbor, EDGE_TREE));
            dfs_visit_node(graph, neighbor, edge_id, options, state, trace);
        } else {
            EdgeType edge_type;

            if (!graph->directed && state->colors[neighbor] != NODE_GRAY) continue;

            if (graph->directed) {
                edge_type = classify_dfs_seen_edge(state, node, neighbor);
            } else {
                edge_type = EDGE_BACK;
                state->edge_classified[edge_id] = 1;
            }

            push_event(trace, make_edge_event(TRACE_EVENT_CLASSIFY_EDGE, edge_id,
                                              node, neighbor, edge_type));
        }
    }

    finish_node(state, trace, node);
}

static void build_dfs_trace(const Graph *graph, const TraversalOptions *options,
                            Trace *trace) {
    TraversalState state;
    if (!traversal_state_init(graph, &state)) return;

    for (int node = first_root_node(graph, options); node != -1;
         node = next_root_node(graph, node, options)) {
        if (state.colors[node] == NODE_WHITE)
            dfs_visit_node(graph, node, -1, options, &state, trace);
    }

    traversal_state_free(&state);
}

/*
 * BFS uses levels as its primary educational state. Tree edges discover new
 * nodes and assign their level; already-reached neighbors are emitted as cross
 * edges for visualization without teaching DFS-style edge classes.
 */
static void build_bfs_trace(const Graph *graph, const TraversalOptions *options,
                            Trace *trace) {
    TraversalState state;
    int *queue = NULL;

    if (!traversal_state_init(graph, &state)) return;
    if (graph->node_count > 0) {
        queue = malloc(graph->node_count * sizeof(*queue));
        if (queue == NULL) {
            traversal_state_free(&state);
            return;
        }
    }

    for (int root = first_root_node(graph, options); root != -1;
         root = next_root_node(graph, root, options)) {
        if (state.colors[root] != NODE_WHITE) continue;

        int head = 0;
        int tail = 0;
        discover_node_at_level(&state, trace, root, 0);
        queue[tail] = root;
        tail++;

        while (head < tail) {
            int node = queue[head];
            head++;

            for (int edge_id = first_out_edge(graph, node, options); edge_id != -1;
                 edge_id = next_out_edge(graph, edge_id, node, options)) {
                int neighbor = graph_edge_neighbor(graph, edge_id, node);

                if (neighbor < 0) continue;
                if (is_reverse_parent_edge(graph, edge_id, state.parent_edges[node]))
                    continue;
                if (!graph->directed && state.edge_classified[edge_id]) continue;

                push_event(trace,
                           make_edge_event(TRACE_EVENT_EXAMINE_EDGE, edge_id, node,
                                           neighbor, EDGE_UNCLASSIFIED));

                if (state.colors[neighbor] == NODE_WHITE) {
                    int neighbor_level = state.levels[node] + 1;
                    state.parent_edges[neighbor] = edge_id;
                    state.edge_classified[edge_id] = 1;
                    push_event(trace, make_edge_event_with_time(
                                          TRACE_EVENT_CLASSIFY_EDGE, edge_id, node,
                                          neighbor, EDGE_TREE, neighbor_level));
                    discover_node_at_level(&state, trace, neighbor, neighbor_level);
                    if ((size_t)tail < graph->node_count) {
                        queue[tail] = neighbor;
                        tail++;
                    }
                } else {
                    state.edge_classified[edge_id] = 1;
                    push_event(trace,
                               make_edge_event_with_time(
                                   TRACE_EVENT_CLASSIFY_EDGE, edge_id, node,
                                   neighbor, EDGE_CROSS, state.levels[neighbor]));
                }
            }

            finish_node(&state, trace, node);
        }
    }

    free(queue);
    traversal_state_free(&state);
}

static bool dijkstra_heap_node_precedes(const DijkstraHeap *heap, int left,
                                        int right) {
    if (heap->distances[left] != heap->distances[right])
        return heap->distances[left] < heap->distances[right];
    return heap->ranks[left] < heap->ranks[right];
}

static void dijkstra_heap_swap(DijkstraHeap *heap, size_t left, size_t right) {
    int left_node = heap->nodes[left];
    int right_node = heap->nodes[right];

    heap->nodes[left] = right_node;
    heap->nodes[right] = left_node;
    heap->positions[left_node] = (int)right;
    heap->positions[right_node] = (int)left;
}

static void dijkstra_heap_sift_up(DijkstraHeap *heap, size_t position) {
    while (position > 0) {
        size_t parent = (position - 1) / 2;
        if (!dijkstra_heap_node_precedes(heap, heap->nodes[position],
                                         heap->nodes[parent]))
            break;
        dijkstra_heap_swap(heap, position, parent);
        position = parent;
    }
}

static void dijkstra_heap_sift_down(DijkstraHeap *heap, size_t position) {
    for (;;) {
        size_t left = position * 2 + 1;
        size_t right = left + 1;
        size_t best = position;

        if (left < heap->count &&
            dijkstra_heap_node_precedes(heap, heap->nodes[left], heap->nodes[best]))
            best = left;
        if (right < heap->count &&
            dijkstra_heap_node_precedes(heap, heap->nodes[right], heap->nodes[best]))
            best = right;
        if (best == position) break;

        dijkstra_heap_swap(heap, position, best);
        position = best;
    }
}

static bool dijkstra_heap_init(DijkstraHeap *heap, size_t node_count,
                               const long long *distances, const int *ranks) {
    memset(heap, 0, sizeof(*heap));
    heap->distances = distances;
    heap->ranks = ranks;
    heap->capacity = node_count;

    if (node_count == 0) return true;

    heap->nodes = malloc(node_count * sizeof(*heap->nodes));
    heap->positions = malloc(node_count * sizeof(*heap->positions));
    if (heap->nodes == NULL || heap->positions == NULL) {
        free(heap->nodes);
        free(heap->positions);
        memset(heap, 0, sizeof(*heap));
        return false;
    }

    for (size_t i = 0; i < node_count; i++) heap->positions[i] = -1;
    return true;
}

static void dijkstra_heap_free(DijkstraHeap *heap) {
    free(heap->nodes);
    free(heap->positions);
    memset(heap, 0, sizeof(*heap));
}

static bool dijkstra_heap_add_or_decrease(DijkstraHeap *heap, int node) {
    int position = heap->positions[node];
    if (position == -2) return false;

    if (position == -1) {
        if (heap->count >= heap->capacity) return false;
        position = (int)heap->count;
        heap->nodes[heap->count] = node;
        heap->positions[node] = position;
        heap->count++;
    }

    dijkstra_heap_sift_up(heap, (size_t)position);
    return true;
}

static int dijkstra_heap_pop(DijkstraHeap *heap) {
    if (heap->count == 0) return -1;

    int node = heap->nodes[0];
    heap->positions[node] = -2;
    heap->count--;

    if (heap->count > 0) {
        heap->nodes[0] = heap->nodes[heap->count];
        heap->positions[heap->nodes[0]] = 0;
        dijkstra_heap_sift_down(heap, 0);
    }

    return node;
}

/*
 * Dijkstra uses the first node in the selected traversal order as its source.
 * Successful relaxations replace the currently highlighted predecessor edge,
 * while settled nodes are final and never re-enter the min-heap.
 */
static void build_dijkstra_trace(const Graph *graph, const TraversalOptions *options,
                                 Trace *trace) {
    if (graph->node_count == 0) return;

    long long *distances = malloc(graph->node_count * sizeof(*distances));
    int *parent_edges = malloc(graph->node_count * sizeof(*parent_edges));
    int *ranks = malloc(graph->node_count * sizeof(*ranks));
    unsigned char *settled = calloc(graph->node_count, sizeof(*settled));
    if (distances == NULL || parent_edges == NULL || ranks == NULL ||
        settled == NULL) {
        free(distances);
        free(parent_edges);
        free(ranks);
        free(settled);
        return;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        distances[i] = GRAPHE_DISTANCE_INFINITY;
        parent_edges[i] = -1;
        ranks[i] = (int)i;
    }

    int rank = 0;
    for (int node = first_root_node(graph, options); node != -1;
         node = next_root_node(graph, node, options)) {
        ranks[node] = rank;
        rank++;
    }

    DijkstraHeap heap;
    if (!dijkstra_heap_init(&heap, graph->node_count, distances, ranks)) {
        free(distances);
        free(parent_edges);
        free(ranks);
        free(settled);
        return;
    }

    int source = first_root_node(graph, options);
    distances[source] = 0;
    push_event(trace, make_distance_event(TRACE_EVENT_SET_DISTANCE, source, 0,
                                          GRAPHE_DISTANCE_INFINITY));
    dijkstra_heap_add_or_decrease(&heap, source);

    while (heap.count > 0) {
        int node = dijkstra_heap_pop(&heap);
        if (node < 0 || settled[node]) continue;

        settled[node] = 1;
        push_event(trace, make_distance_event(TRACE_EVENT_SETTLE_NODE, node,
                                              distances[node], distances[node]));

        for (int edge_id = first_out_edge(graph, node, options); edge_id != -1;
             edge_id = next_out_edge(graph, edge_id, node, options)) {
            int neighbor = graph_edge_neighbor(graph, edge_id, node);
            if (neighbor < 0) continue;

            push_event(trace, make_edge_event(TRACE_EVENT_EXAMINE_EDGE, edge_id,
                                              node, neighbor, EDGE_UNCLASSIFIED));

            const Edge *edge = &graph->edges[edge_id];
            if (settled[neighbor] || edge->weight < 0 ||
                distances[node] == GRAPHE_DISTANCE_INFINITY ||
                (long long)edge->weight > GRAPHE_DISTANCE_INFINITY - distances[node])
                continue;

            long long candidate = distances[node] + edge->weight;
            if (distances[neighbor] != GRAPHE_DISTANCE_INFINITY &&
                candidate >= distances[neighbor])
                continue;

            long long old_distance = distances[neighbor];
            int old_parent_edge = parent_edges[neighbor];
            distances[neighbor] = candidate;
            parent_edges[neighbor] = edge_id;
            push_event(trace, make_relax_event(edge_id, node, neighbor, candidate,
                                               old_distance, old_parent_edge));
            dijkstra_heap_add_or_decrease(&heap, neighbor);
        }
    }

    dijkstra_heap_free(&heap);
    free(distances);
    free(parent_edges);
    free(ranks);
    free(settled);
}

/*
 * Tree traversal deliberately uses insertion-order children. Tree files are
 * ordered examples, and alphabetical sorting would change the printed sequence
 * the user is trying to study.
 */
static void tree_visit_node(const Graph *graph, int node,
                            const TraversalOptions *options, TraversalState *state,
                            Trace *trace) {
    int visited_node = 0;
    int child_index = 0;

    if (options->tree_order == TREE_ORDER_PREORDER) {
        discover_node(state, trace, node);
        visited_node = 1;
    }

    for (int edge_id = first_out_edge_in_insertion_order(graph, node); edge_id != -1;
         edge_id = next_out_edge_in_insertion_order(graph, edge_id, node)) {
        int neighbor = graph_edge_neighbor(graph, edge_id, node);
        if (neighbor < 0) continue;

        if (options->tree_order == TREE_ORDER_INORDER && child_index == 1) {
            discover_node(state, trace, node);
            visited_node = 1;
        }
        child_index++;

        if (state->colors[neighbor] != NODE_WHITE) continue;

        push_event(trace, make_edge_event(TRACE_EVENT_EXAMINE_EDGE, edge_id, node,
                                          neighbor, EDGE_UNCLASSIFIED));
        push_event(trace, make_edge_event(TRACE_EVENT_CLASSIFY_EDGE, edge_id, node,
                                          neighbor, EDGE_TREE));
        tree_visit_node(graph, neighbor, options, state, trace);
    }

    if (options->tree_order == TREE_ORDER_INORDER && !visited_node) {
        discover_node(state, trace, node);
    }
    if (options->tree_order == TREE_ORDER_POSTORDER)
        discover_node(state, trace, node);
}

/*
 * Finds structural roots for tree mode instead of relying on label order, then
 * visits disconnected leftovers defensively so malformed in-memory samples still
 * produce a complete trace.
 */
static void build_tree_trace(const Graph *graph, const TraversalOptions *options,
                             Trace *trace) {
    TraversalState state;
    if (!traversal_state_init(graph, &state)) return;

    int *has_parent = NULL;
    if (graph->node_count > 0) {
        has_parent = calloc(graph->node_count, sizeof(*has_parent));
        if (has_parent == NULL) {
            traversal_state_free(&state);
            return;
        }
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        int child = graph->edges[i].to;
        if (child >= 0 && (size_t)child < graph->node_count) has_parent[child] = 1;
    }

    for (size_t node = 0; node < graph->node_count; node++) {
        if (!has_parent[node] && state.colors[node] == NODE_WHITE)
            tree_visit_node(graph, (int)node, options, &state, trace);
    }
    for (size_t node = 0; node < graph->node_count; node++) {
        if (state.colors[node] == NODE_WHITE)
            tree_visit_node(graph, (int)node, options, &state, trace);
    }

    free(has_parent);
    traversal_state_free(&state);
}

/*
 * Builds a mode-specific trace without exposing rendering concerns to traversal
 * code. The renderer later applies a prefix of this trace for playback.
 */
void traversal_trace_build(const Graph *graph, const TraversalOptions *options,
                           Trace *trace) {
    if (options == NULL) options = &default_options;

    trace->count = 0;

    switch (options->algorithm) {
    case ALGORITHM_BFS:
        build_bfs_trace(graph, options, trace);
        break;
    case ALGORITHM_DIJKSTRA:
        build_dijkstra_trace(graph, options, trace);
        break;
    case ALGORITHM_TREE:
        build_tree_trace(graph, options, trace);
        break;
    case ALGORITHM_DFS:
    default:
        build_dfs_trace(graph, options, trace);
        break;
    }
}

/*
 * Replays the first event_count trace events onto a fresh visual copy of the
 * base graph. This is what makes jumping backward cheap and deterministic:
 * rebuild state from the trace instead of undoing events.
 */
void traversal_trace_apply_prefix(const Graph *base, const Trace *trace,
                                  size_t event_count, Graph *out) {
    if (!graph_structure_equals(out, base)) {
        if (!graph_copy(base, out)) return;
    } else {
        graph_copy_node_positions(out, base);
    }
    graph_reset_visual_state(out);

    if (event_count > trace->count) event_count = trace->count;

    for (size_t i = 0; i < event_count; i++) {
        const TraceEvent *event = &trace->events[i];

        switch (event->type) {
        case TRACE_EVENT_DISCOVER_NODE:
            out->nodes[event->node].color = NODE_GRAY;
            out->nodes[event->node].discover_time = event->time;
            out->nodes[event->node].level = event->time;
            break;
        case TRACE_EVENT_EXAMINE_EDGE:
            break;
        case TRACE_EVENT_CLASSIFY_EDGE:
            out->edges[event->edge].type = event->edge_type;
            break;
        case TRACE_EVENT_FINISH_NODE:
            out->nodes[event->node].color = NODE_BLACK;
            out->nodes[event->node].finish_time = event->time;
            break;
        case TRACE_EVENT_SET_DISTANCE:
            out->nodes[event->node].color = NODE_GRAY;
            out->nodes[event->node].distance = event->distance;
            break;
        case TRACE_EVENT_RELAX_EDGE:
            if (event->replaced_edge >= 0 &&
                (size_t)event->replaced_edge < out->edge_count)
                out->edges[event->replaced_edge].type = EDGE_UNCLASSIFIED;
            out->edges[event->edge].type = EDGE_TREE;
            out->nodes[event->to].distance = event->distance;
            if (out->nodes[event->to].color == NODE_WHITE)
                out->nodes[event->to].color = NODE_GRAY;
            break;
        case TRACE_EVENT_SETTLE_NODE:
            out->nodes[event->node].distance = event->distance;
            out->nodes[event->node].color = NODE_BLACK;
            break;
        default:
            break;
        }
    }
}
