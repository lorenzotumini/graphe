#include "layout.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void layout_circle(Graph *graph, float center_x, float center_y, float radius) {
    const float two_pi = 6.28318530718f;

    if (graph->node_count == 0) return;

    for (size_t i = 0; i < graph->node_count; i++) {
        float angle = -1.57079632679f + two_pi * (float)i / (float)graph->node_count;
        graph->nodes[i].x = center_x + cosf(angle) * radius;
        graph->nodes[i].y = center_y + sinf(angle) * radius;
    }
}

typedef struct ForestLayoutState {
    int *first_child;
    int *last_child;
    int *next_sibling;
    int *child_counts;
    int *roots;
    int root_count;
    int *parent;
    int *selected_parent_edge;
    int *subtree_leaves;
    size_t node_count;
} ForestLayoutState;

static void forest_layout_state_free(ForestLayoutState *state) {
    free(state->first_child);
    free(state->last_child);
    free(state->next_sibling);
    free(state->child_counts);
    free(state->roots);
    free(state->parent);
    free(state->selected_parent_edge);
    free(state->subtree_leaves);
    memset(state, 0, sizeof(*state));
}

static bool forest_layout_state_init(ForestLayoutState *state, size_t node_count) {
    memset(state, 0, sizeof(*state));
    state->node_count = node_count;

    if (node_count == 0) return true;

    state->first_child = malloc(node_count * sizeof(*state->first_child));
    state->last_child = malloc(node_count * sizeof(*state->last_child));
    state->next_sibling = malloc(node_count * sizeof(*state->next_sibling));
    state->child_counts = calloc(node_count, sizeof(*state->child_counts));
    state->roots = malloc(node_count * sizeof(*state->roots));
    state->parent = malloc(node_count * sizeof(*state->parent));
    state->selected_parent_edge =
        malloc(node_count * sizeof(*state->selected_parent_edge));
    state->subtree_leaves = malloc(node_count * sizeof(*state->subtree_leaves));

    if (state->first_child == NULL || state->last_child == NULL ||
        state->next_sibling == NULL || state->child_counts == NULL ||
        state->roots == NULL || state->parent == NULL ||
        state->selected_parent_edge == NULL || state->subtree_leaves == NULL) {
        forest_layout_state_free(state);
        return false;
    }

    for (size_t i = 0; i < node_count; i++) {
        state->first_child[i] = -1;
        state->last_child[i] = -1;
        state->next_sibling[i] = -1;
        state->parent[i] = -1;
        state->selected_parent_edge[i] = -1;
        state->subtree_leaves[i] = 1;
    }

    return true;
}

static void append_child(ForestLayoutState *state, int parent, int child) {
    if (state->parent[child] != -1) return;

    state->parent[child] = parent;
    state->next_sibling[child] = -1;

    if (state->first_child[parent] == -1) {
        state->first_child[parent] = child;
    } else {
        state->next_sibling[state->last_child[parent]] = child;
    }

    state->last_child[parent] = child;
    state->child_counts[parent]++;
}

/*
 * Derives a forest from EDGE_TREE classification events instead of from raw
 * graph edges. That keeps the layout aligned with what the current traversal is
 * teaching, even when the graph has many non-tree edges.
 */
static void build_forest_from_trace(const Graph *graph, const Trace *trace,
                                    ForestLayoutState *state) {
    for (size_t i = 0; i < trace->count; i++) {
        const TraceEvent *event = &trace->events[i];

        bool selects_parent = (event->type == TRACE_EVENT_CLASSIFY_EDGE &&
                               event->edge_type == EDGE_TREE) ||
                              event->type == TRACE_EVENT_RELAX_EDGE;
        if (!selects_parent || event->to < 0 ||
            (size_t)event->to >= graph->node_count)
            continue;

        state->selected_parent_edge[event->to] = event->edge;
    }

    for (size_t i = 0; i < trace->count; i++) {
        const TraceEvent *event = &trace->events[i];
        bool selects_parent = (event->type == TRACE_EVENT_CLASSIFY_EDGE &&
                               event->edge_type == EDGE_TREE) ||
                              event->type == TRACE_EVENT_RELAX_EDGE;

        if (!selects_parent || event->from < 0 || event->to < 0) continue;
        if ((size_t)event->from >= graph->node_count ||
            (size_t)event->to >= graph->node_count)
            continue;
        if (state->selected_parent_edge[event->to] != event->edge) continue;

        append_child(state, event->from, event->to);
    }

    for (size_t i = 0; i < trace->count; i++) {
        const TraceEvent *event = &trace->events[i];

        if (event->type != TRACE_EVENT_DISCOVER_NODE &&
            event->type != TRACE_EVENT_SET_DISTANCE &&
            event->type != TRACE_EVENT_SETTLE_NODE)
            continue;
        if (event->node < 0 || (size_t)event->node >= graph->node_count) continue;
        if (state->parent[event->node] != -1) continue;
        if ((size_t)state->root_count >= state->node_count) continue;

        bool already_added = false;
        for (int root = 0; root < state->root_count; root++) {
            if (state->roots[root] == event->node) already_added = true;
        }
        if (already_added) continue;

        state->roots[state->root_count] = event->node;
        state->root_count++;
    }

    for (size_t node = 0; node < graph->node_count; node++) {
        if (state->parent[node] != -1) continue;

        bool already_added = false;
        for (int root = 0; root < state->root_count; root++) {
            if (state->roots[root] == (int)node) already_added = true;
        }
        if (already_added) continue;

        state->roots[state->root_count] = (int)node;
        state->root_count++;
    }
}

/*
 * Counts leaves in each traversal subtree so horizontal space can be allocated
 * by visible breadth rather than by raw node count.
 */
static int measure_subtree(int node, ForestLayoutState *state) {
    int leaves = 0;

    if (state->child_counts[node] == 0) {
        state->subtree_leaves[node] = 1;
        return 1;
    }

    for (int child = state->first_child[node]; child != -1;
         child = state->next_sibling[child]) {
        leaves += measure_subtree(child, state);
    }

    state->subtree_leaves[node] = leaves;
    return leaves;
}

static float node_jitter(int node, int axis, float amount) {
    unsigned int value = (unsigned int)(node + 1) * 1103515245u;
    value ^= (unsigned int)(axis + 3) * 2654435761u;
    value = (value >> 16) & 1023u;
    return (((float)value / 1023.0f) - 0.5f) * amount;
}

/*
 * Places one measured subtree in its horizontal slot. The small deterministic
 * jitter keeps large examples from looking perfectly mechanical without making
 * the layout change between frames.
 */
static void place_subtree(Graph *graph, ForestLayoutState *state, int node,
                          int depth, float slot_width, float level_gap, float left,
                          float top, float *cursor) {
    float subtree_width = (float)state->subtree_leaves[node] * slot_width;
    float jitter_scale = depth == 0 ? 0.35f : 1.0f;
    graph->nodes[node].x = left + *cursor + subtree_width * 0.5f +
                           node_jitter(node, 0, 30.0f) * jitter_scale;
    graph->nodes[node].y =
        top + (float)depth * level_gap + node_jitter(node, 1, 18.0f);

    for (int child = state->first_child[node]; child != -1;
         child = state->next_sibling[child]) {
        place_subtree(graph, state, child, depth + 1, slot_width, level_gap, left,
                      top, cursor);
    }

    if (state->child_counts[node] == 0) {
        *cursor += slot_width;
        return;
    }

    float child_leaves = 0.0f;
    for (int child = state->first_child[node]; child != -1;
         child = state->next_sibling[child]) {
        child_leaves += (float)state->subtree_leaves[child];
    }
    *cursor += subtree_width - child_leaves * slot_width;
}

static bool edge_belongs_to_forest(const ForestLayoutState *state, int edge_index) {
    for (size_t node = 0; node < state->node_count; node++) {
        if (state->selected_parent_edge[node] == edge_index) return true;
    }

    return false;
}

static float clamp_layout_x(float x, float left, float right) {
    const float node_margin = 38.0f;
    float min_x = left + node_margin;
    float max_x = right - node_margin;

    if (max_x < min_x) return (left + right) * 0.5f;
    if (x < min_x) return min_x;
    if (x > max_x) return max_x;
    return x;
}

/*
 * Moves nodes out of non-tree shortcut edges that pass through them. Only the
 * horizontal coordinate changes, so traversal levels remain visually stable.
 * Multiple passes cover chains where moving one node changes a later shortcut.
 */
static void separate_nodes_from_non_tree_edges(Graph *graph,
                                               const ForestLayoutState *state,
                                               float left, float width) {
    const float clearance = 48.0f;
    const float max_shift = 72.0f;
    float right = left + width;

    for (int pass = 0; pass < 3; pass++) {
        for (size_t edge_index = 0; edge_index < graph->edge_count; edge_index++) {
            if (edge_belongs_to_forest(state, (int)edge_index)) continue;

            const Edge *edge = &graph->edges[edge_index];
            if (edge->from == edge->to) continue;

            const Node *from = &graph->nodes[edge->from];
            const Node *to = &graph->nodes[edge->to];
            float dx = to->x - from->x;
            float dy = to->y - from->y;
            float length_squared = dx * dx + dy * dy;
            if (length_squared < 1.0f) continue;

            float length = sqrtf(length_squared);
            float normal_x = -dy / length;
            if (fabsf(normal_x) < 0.35f) continue;

            for (size_t node_index = 0; node_index < graph->node_count;
                 node_index++) {
                if ((int)node_index == edge->from || (int)node_index == edge->to)
                    continue;

                Node *node = &graph->nodes[node_index];
                float relative_x = node->x - from->x;
                float relative_y = node->y - from->y;
                float projection =
                    (relative_x * dx + relative_y * dy) / length_squared;
                if (projection <= 0.12f || projection >= 0.88f) continue;

                float closest_x = from->x + dx * projection;
                float closest_y = from->y + dy * projection;
                float signed_distance = (node->x - closest_x) * normal_x +
                                        (node->y - closest_y) * (dx / length);
                if (fabsf(signed_distance) >= clearance) continue;

                float side =
                    signed_distance < -0.5f ? -1.0f
                    : signed_distance > 0.5f
                        ? 1.0f
                        : ((node_index + edge_index) % 2 == 0 ? -1.0f : 1.0f);
                float shift = (side * clearance - signed_distance) / normal_x;
                if (shift < -max_shift) shift = -max_shift;
                if (shift > max_shift) shift = max_shift;

                float shifted_x = clamp_layout_x(node->x + shift, left, right);
                if (fabsf(shifted_x - node->x) < 1.0f) {
                    shifted_x = clamp_layout_x(node->x - shift, left, right);
                }
                node->x = shifted_x;
            }
        }
    }
}

static bool nodes_have_non_tree_edge(const Graph *graph,
                                     const ForestLayoutState *state, int left,
                                     int right) {
    for (size_t edge_index = 0; edge_index < graph->edge_count; edge_index++) {
        const Edge *edge = &graph->edges[edge_index];
        if (edge_belongs_to_forest(state, (int)edge_index)) continue;

        if ((edge->from == left && edge->to == right) ||
            (edge->from == right && edge->to == left))
            return true;
    }

    return false;
}

/*
 * A shortcut from a node to its grandchild otherwise sits directly over both
 * edges of a unary tree chain. Resolve this common case last so other edge
 * nudges cannot pull the intermediate node back onto the shortcut.
 */
static void separate_unary_chain_shortcuts(Graph *graph,
                                           const ForestLayoutState *state,
                                           float left, float width) {
    const float clearance = 52.0f;
    float right = left + width;

    for (size_t parent = 0; parent < state->node_count; parent++) {
        if (state->child_counts[parent] != 1) continue;

        int middle = state->first_child[parent];
        if (middle < 0 || state->child_counts[middle] != 1) continue;

        int grandchild = state->first_child[middle];
        if (grandchild < 0 ||
            !nodes_have_non_tree_edge(graph, state, (int)parent, grandchild))
            continue;

        const Node *from = &graph->nodes[parent];
        const Node *to = &graph->nodes[grandchild];
        Node *node = &graph->nodes[middle];
        float dx = to->x - from->x;
        float dy = to->y - from->y;
        float length = sqrtf(dx * dx + dy * dy);
        if (length < 1.0f) continue;

        float normal_x = -dy / length;
        if (fabsf(normal_x) < 0.25f) continue;

        float projection = ((node->x - from->x) * dx + (node->y - from->y) * dy) /
                           (length * length);
        float closest_x = from->x + dx * projection;
        float closest_y = from->y + dy * projection;
        float signed_distance =
            (node->x - closest_x) * normal_x + (node->y - closest_y) * (dx / length);

        float negative_x = clamp_layout_x(
            node->x + (-clearance - signed_distance) / normal_x, left, right);
        float positive_x = clamp_layout_x(
            node->x + (clearance - signed_distance) / normal_x, left, right);
        float negative_shift = fabsf(negative_x - node->x);
        float positive_shift = fabsf(positive_x - node->x);

        node->x = negative_shift < positive_shift ? negative_x : positive_x;
    }
}

/*
 * Main traversal-forest layout entry point. It falls back to a circular layout
 * when no tree edges are available, which covers empty or not-yet-classified
 * traces.
 */
void layout_trace_forest(Graph *graph, const Trace *trace, float left, float top,
                         float width, float level_gap) {
    if (graph->node_count == 0) return;

    ForestLayoutState state;
    if (!forest_layout_state_init(&state, graph->node_count)) {
        layout_circle(graph, left + width * 0.5f, top + level_gap, width * 0.25f);
        return;
    }

    build_forest_from_trace(graph, trace, &state);

    if (state.root_count == 0) {
        layout_circle(graph, left + width * 0.5f, top + level_gap, width * 0.25f);
        forest_layout_state_free(&state);
        return;
    }

    int total_leaves = 0;
    for (int i = 0; i < state.root_count; i++)
        total_leaves += measure_subtree(state.roots[i], &state);

    if (total_leaves <= 0) total_leaves = 1;

    float slot_width = width / (float)total_leaves;
    float cursor = 0.0f;

    for (int i = 0; i < state.root_count; i++)
        place_subtree(graph, &state, state.roots[i], 0, slot_width, level_gap, left,
                      top, &cursor);

    separate_nodes_from_non_tree_edges(graph, &state, left, width);
    separate_unary_chain_shortcuts(graph, &state, left, width);

    forest_layout_state_free(&state);
}
