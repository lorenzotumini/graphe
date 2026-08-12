#include "graph_io.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TreeIdMap {
    char **ids;
    int *nodes;
    size_t count;
    size_t capacity;
} TreeIdMap;

static void set_error(char *error, size_t error_size, const char *message,
                      int line_number) {
    if (error == NULL || error_size == 0) return;

    if (line_number > 0) {
        snprintf(error, error_size, "line %d: %s", line_number, message);
    } else {
        snprintf(error, error_size, "%s", message);
    }
}

static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy == NULL) return NULL;

    memcpy(copy, text, length);
    return copy;
}

static char *trim_left(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    return text;
}

static void trim_right(char *text) {
    size_t length = strlen(text);

    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[length - 1] = '\0';
        length--;
    }
}

static char *read_token(char **cursor) {
    char *text = trim_left(*cursor);
    if (*text == '\0') {
        *cursor = text;
        return NULL;
    }

    char *token = text;
    while (*text != '\0' && !isspace((unsigned char)*text)) text++;

    if (*text != '\0') {
        *text = '\0';
        text++;
    }

    *cursor = text;
    return token;
}

static bool has_extra_token(char *cursor) {
    cursor = trim_left(cursor);
    return *cursor != '\0';
}

static bool graph_label_is_valid(const char *label) {
    return label != NULL && label[0] != '\0';
}

static bool parse_edge_weight(const char *text, int *weight, char *error,
                              size_t error_size, int line_number) {
    if (text == NULL) {
        *weight = 1;
        return true;
    }

    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed < 0 ||
        parsed > INT_MAX) {
        set_error(error, error_size, "edge weight must be a non-negative integer",
                  line_number);
        return false;
    }

    *weight = (int)parsed;
    return true;
}

static int find_or_add_graph_node(Graph *graph, const char *label, char *error,
                                  size_t error_size, int line_number) {
    if (!graph_label_is_valid(label)) {
        set_error(error, error_size, "invalid node label", line_number);
        return -1;
    }

    int existing = graph_find_node(graph, label);
    if (existing >= 0) return existing;

    int node = graph_add_node(graph, label);
    if (node < 0) set_error(error, error_size, "could not add node", line_number);

    return node;
}

static void tree_id_map_free(TreeIdMap *map) {
    for (size_t i = 0; i < map->count; i++) free(map->ids[i]);

    free((void *)map->ids);
    free(map->nodes);
    memset(map, 0, sizeof(*map));
}

static bool tree_id_map_reserve(TreeIdMap *map, size_t required) {
    if (required <= map->capacity) return true;

    size_t capacity = map->capacity == 0 ? 16 : map->capacity;
    while (capacity < required) capacity *= 2;

    char **ids = (char **)realloc((void *)map->ids, capacity * sizeof(*ids));
    if (ids == NULL) return false;
    map->ids = ids;

    int *nodes = realloc(map->nodes, capacity * sizeof(*nodes));
    if (nodes == NULL) return false;
    map->nodes = nodes;

    map->capacity = capacity;
    return true;
}

static int tree_id_map_find(const TreeIdMap *map, const char *id) {
    for (size_t i = 0; i < map->count; i++) {
        if (strcmp(map->ids[i], id) == 0) return map->nodes[i];
    }

    return -1;
}

static bool tree_id_map_add(TreeIdMap *map, const char *id, int node) {
    if (!tree_id_map_reserve(map, map->count + 1)) return false;

    char *id_copy = copy_string(id);
    if (id_copy == NULL) return false;

    map->ids[map->count] = id_copy;
    map->nodes[map->count] = node;
    map->count++;
    return true;
}

/*
 * Tree files use stable IDs for edges and separate display values for nodes.
 * This lets a node print arbitrary text while edges still reference compact,
 * unambiguous identifiers.
 */
static bool add_tree_node(Graph *graph, TreeIdMap *map, const char *id,
                          const char *value, char *error, size_t error_size,
                          int line_number) {
    if (id == NULL || id[0] == '\0') {
        set_error(error, error_size, "tree node line must include an id",
                  line_number);
        return false;
    }
    if (tree_id_map_find(map, id) >= 0) {
        set_error(error, error_size, "duplicate tree node id", line_number);
        return false;
    }
    if (value == NULL || value[0] == '\0') value = id;

    int node = graph_add_node(graph, value);
    if (node < 0) {
        set_error(error, error_size, "could not add tree node", line_number);
        return false;
    }
    if (!tree_id_map_add(map, id, node)) {
        set_error(error, error_size, "could not store tree node id", line_number);
        return false;
    }

    return true;
}

static int require_tree_node(const TreeIdMap *map, const char *id, char *error,
                             size_t error_size, int line_number) {
    int node = tree_id_map_find(map, id);
    if (node < 0)
        set_error(error, error_size, "tree edge references unknown node id",
                  line_number);
    return node;
}

/*
 * Validates the imported tree as a rooted directed tree: exactly one root, one
 * parent per non-root node, and all nodes reachable from that root.
 */
static bool validate_tree_file(const Graph *graph, char *error, size_t error_size) {
    if (graph->node_count == 0) {
        set_error(error, error_size, "tree file contains no nodes", 0);
        return false;
    }
    if (graph->edge_count + 1 != graph->node_count) {
        set_error(error, error_size,
                  "tree must have exactly one fewer edge than nodes", 0);
        return false;
    }

    int *parent = malloc(graph->node_count * sizeof(*parent));
    int *visited = calloc(graph->node_count, sizeof(*visited));
    int *stack = malloc(graph->node_count * sizeof(*stack));
    if (parent == NULL || visited == NULL || stack == NULL) {
        free(parent);
        free(visited);
        free(stack);
        set_error(error, error_size, "out of memory while validating tree", 0);
        return false;
    }

    for (size_t i = 0; i < graph->node_count; i++) parent[i] = -1;

    for (size_t i = 0; i < graph->edge_count; i++) {
        int child = graph->edges[i].to;
        if (parent[child] != -1) {
            free(parent);
            free(visited);
            free(stack);
            set_error(error, error_size, "tree node has more than one parent", 0);
            return false;
        }
        parent[child] = graph->edges[i].from;
    }

    int root = -1;
    int root_count = 0;
    for (size_t i = 0; i < graph->node_count; i++) {
        if (parent[i] == -1) {
            root = (int)i;
            root_count++;
        }
    }
    if (root_count != 1) {
        free(parent);
        free(visited);
        free(stack);
        set_error(error, error_size, "tree file must contain exactly one root", 0);
        return false;
    }

    int top = 0;
    size_t reached = 0;
    stack[top++] = root;
    visited[root] = 1;

    while (top > 0) {
        int node = stack[--top];
        reached++;

        for (int edge_id = graph->first_out[node]; edge_id != -1;
             edge_id = graph_next_adjacent_edge(graph, edge_id, node, false)) {
            int child = graph->edges[edge_id].to;
            if (visited[child]) continue;

            visited[child] = 1;
            stack[top++] = child;
        }
    }

    free(parent);
    free(visited);
    free(stack);

    if (reached != graph->node_count) {
        set_error(error, error_size, "tree must be connected and acyclic", 0);
        return false;
    }

    return true;
}

/*
 * Loads both graph and tree .graphe files into the shared Graph structure. The
 * function builds into a temporary graph first, then swaps it into the output
 * only after the file has parsed and validated successfully.
 */
bool graph_load_from_file(const char *path, Graph *graph, char *error,
                          size_t error_size, GraphFileKind *kind) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        set_error(error, error_size, "could not open graph file", 0);
        return false;
    }

    Graph loaded;
    graph_init(&loaded);
    TreeIdMap tree_ids = {0};
    GraphFileKind loaded_kind = GRAPH_FILE_GRAPH;
    bool tree_edges_started = false;

    char line[2048];
    int line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            set_error(error, error_size, "line is too long", line_number);
            fclose(file);
            graph_free(&loaded);
            tree_id_map_free(&tree_ids);
            return false;
        }

        char *comment = strchr(line, '#');
        if (comment != NULL) *comment = '\0';

        char *content = trim_left(line);
        trim_right(content);
        if (content[0] == '\0') continue;

        char *cursor = content;
        const char *command = read_token(&cursor);

        if (strcmp(command, "tree") == 0) {
            if (has_extra_token(cursor)) {
                set_error(error, error_size, "tree line must be just: tree",
                          line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            if (loaded.node_count > 0 || loaded.edge_count > 0) {
                set_error(error, error_size,
                          "tree must be declared before node or edge lines",
                          line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            loaded_kind = GRAPH_FILE_TREE;
            loaded.directed = true;
            continue;
        }

        if (strcmp(command, "directed") == 0 || strcmp(command, "undirected") == 0) {
            if (loaded_kind == GRAPH_FILE_TREE) {
                set_error(error, error_size,
                          "tree files are always directed from parent to child",
                          line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            if (has_extra_token(cursor)) {
                set_error(error, error_size,
                          "direction line must be just directed or undirected",
                          line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            if (loaded.edge_count > 0) {
                set_error(error, error_size,
                          "direction must be declared before edge lines",
                          line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }

            loaded.directed = strcmp(command, "directed") == 0;
            continue;
        }

        if (strcmp(command, "node") == 0) {
            const char *first = read_token(&cursor);
            if (loaded_kind == GRAPH_FILE_TREE) {
                if (tree_edges_started) {
                    set_error(error, error_size,
                              "tree node lines must come before edge lines",
                              line_number);
                    fclose(file);
                    graph_free(&loaded);
                    tree_id_map_free(&tree_ids);
                    return false;
                }

                char *value = trim_left(cursor);
                trim_right(value);
                if (!add_tree_node(&loaded, &tree_ids, first, value, error,
                                   error_size, line_number)) {
                    fclose(file);
                    graph_free(&loaded);
                    tree_id_map_free(&tree_ids);
                    return false;
                }
                continue;
            }

            if (first == NULL || has_extra_token(cursor)) {
                set_error(error, error_size, "node line must be: node LABEL",
                          line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            if (graph_find_node(&loaded, first) >= 0) {
                set_error(error, error_size, "duplicate node label", line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            if (find_or_add_graph_node(&loaded, first, error, error_size,
                                       line_number) < 0) {
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            continue;
        }

        if (strcmp(command, "edge") == 0) {
            const char *first = read_token(&cursor);
            const char *second = read_token(&cursor);
            const char *weight_text = read_token(&cursor);
            if (first == NULL || second == NULL || has_extra_token(cursor)) {
                set_error(error, error_size,
                          "edge line must be: edge FROM TO [WEIGHT]", line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }

            int weight;
            if (!parse_edge_weight(weight_text, &weight, error, error_size,
                                   line_number)) {
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }

            int from;
            int to;
            if (loaded_kind == GRAPH_FILE_TREE) {
                tree_edges_started = true;
                from = require_tree_node(&tree_ids, first, error, error_size,
                                         line_number);
                to = require_tree_node(&tree_ids, second, error, error_size,
                                       line_number);
            } else {
                from = find_or_add_graph_node(&loaded, first, error, error_size,
                                              line_number);
                to = find_or_add_graph_node(&loaded, second, error, error_size,
                                            line_number);
            }

            if (from < 0 || to < 0) {
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }

            if (graph_add_weighted_edge(&loaded, from, to, weight) < 0) {
                set_error(error, error_size, "could not add edge", line_number);
                fclose(file);
                graph_free(&loaded);
                tree_id_map_free(&tree_ids);
                return false;
            }
            continue;
        }

        set_error(error, error_size, "unknown graph file command", line_number);
        fclose(file);
        graph_free(&loaded);
        tree_id_map_free(&tree_ids);
        return false;
    }

    fclose(file);

    if (loaded.node_count == 0) {
        set_error(error, error_size, "file contains no nodes", 0);
        graph_free(&loaded);
        tree_id_map_free(&tree_ids);
        return false;
    }
    if (loaded_kind == GRAPH_FILE_TREE &&
        !validate_tree_file(&loaded, error, error_size)) {
        graph_free(&loaded);
        tree_id_map_free(&tree_ids);
        return false;
    }

    tree_id_map_free(&tree_ids);
    graph_free(graph);
    *graph = loaded;
    if (kind != NULL) *kind = loaded_kind;
    if (error != NULL && error_size > 0) error[0] = '\0';
    return true;
}
