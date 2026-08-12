#include "render.h"

#include "raygui.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Theme {
    Color background;
    Color panel;
    Color panel_border;
    Color text;
    Color muted_text;
    Color node_white;
    Color node_gray;
    Color node_black;
    Color node_outline;
    Color active;
    Color edge_unclassified;
    Color edge_tree;
    Color edge_back;
    Color edge_forward;
    Color edge_cross;
    Color button;
    Color button_hover;
    Color button_active;
    Color overlay;
} Theme;

typedef struct EdgeGeometry {
    Vector2 start;
    Vector2 control;
    Vector2 end;
    Vector2 end_direction;
    float bend;
} EdgeGeometry;

typedef struct EdgeTopologyCurve {
    bool separated;
    float bend;
    float sign;
} EdgeTopologyCurve;

struct EdgeCurveCacheEntry {
    bool valid;
    int from;
    int to;
    EdgeType type;
    float sign;
};

static Theme light_theme(void) {
    return (Theme){
        .background = {250, 250, 249, 255},
        .panel = {245, 247, 250, 255},
        .panel_border = {209, 213, 219, 255},
        .text = {17, 24, 39, 255},
        .muted_text = {75, 85, 99, 255},
        .node_white = {255, 255, 255, 255},
        .node_gray = {255, 202, 58, 255},
        .node_black = {75, 85, 99, 255},
        .node_outline = {31, 41, 55, 255},
        .active = {249, 115, 22, 255},
        .edge_unclassified = {156, 163, 175, 255},
        .edge_tree = {22, 163, 74, 255},
        .edge_back = {239, 68, 68, 255},
        .edge_forward = {37, 99, 235, 255},
        .edge_cross = {147, 51, 234, 255},
        .button = {229, 231, 235, 255},
        .button_hover = {209, 213, 219, 255},
        .button_active = {191, 219, 254, 255},
        .overlay = {17, 24, 39, 170},
    };
}

static Theme dark_theme(void) {
    return (Theme){
        .background = {17, 24, 39, 255},
        .panel = {31, 41, 55, 255},
        .panel_border = {105, 115, 129, 255},
        .text = {249, 250, 252, 255},
        .muted_text = {182, 189, 201, 255},
        .node_white = {233, 234, 236, 255},
        .node_gray = {225, 118, 21, 255},
        // .node_gray = {192, 185, 74, 255},
        .node_black = {2, 12, 25, 255},
        .node_outline = {107, 114, 128, 255},
        .active = {225, 118, 21, 255},
        // .active = {192, 185, 74, 255},
        .edge_unclassified = {107, 114, 128, 255},
        // .edge_tree = {19, 133, 63, 255},
        .edge_tree = {34, 197, 94, 255},
        .edge_back = {248, 93, 93, 255},
        .edge_forward = {96, 165, 250, 255},
        .edge_cross = {192, 132, 252, 255},
        .button = {87, 87, 93, 255},
        .button_hover = {75, 85, 99, 255},
        .button_active = {30, 64, 175, 255},
        .overlay = {3, 7, 18, 190},
    };
}

static Theme theme_for_options(const RenderOptions *options) {
    if (options->dark_mode) return dark_theme();
    return light_theme();
}

static float ui_scale_value(const RenderOptions *options) {
    if (options == NULL || options->ui_scale <= 0.0f) return 1.0f;
    if (options->ui_scale < GRAPHE_UI_SCALE_MIN) return GRAPHE_UI_SCALE_MIN;
    if (options->ui_scale > GRAPHE_UI_SCALE_MAX) return GRAPHE_UI_SCALE_MAX;
    return options->ui_scale;
}

static float ui_pixel_scale(const RenderOptions *options) {
    return ui_scale_value(options) * GRAPHE_UI_PIXEL_SCALE;
}

static float ui_size(const RenderOptions *options, float value) {
    return value * ui_pixel_scale(options);
}

static bool change_ui_scale(RenderOptions *options, float delta) {
    float next_scale = options->ui_scale + delta;

    if (next_scale < GRAPHE_UI_SCALE_MIN) next_scale = GRAPHE_UI_SCALE_MIN;
    if (next_scale > GRAPHE_UI_SCALE_MAX) next_scale = GRAPHE_UI_SCALE_MAX;
    if (next_scale == options->ui_scale) return false;

    options->ui_scale = next_scale;
    return true;
}

float render_sidebar_width(const RenderOptions *options) {
    float sidebar_width = GRAPHE_SIDEBAR_WIDTH * ui_pixel_scale(options);
    float max_sidebar_width = (float)GetScreenWidth() - 320.0f;

    if (max_sidebar_width < 280.0f) max_sidebar_width = 280.0f;
    if (sidebar_width > max_sidebar_width) sidebar_width = max_sidebar_width;

    return sidebar_width;
}

float render_graph_area_width(const RenderOptions *options) {
    return (float)GetScreenWidth() - render_sidebar_width(options);
}

float render_graph_area_height(void) {
    return (float)GetScreenHeight();
}

static float sidebar_left(const RenderOptions *options) {
    return render_graph_area_width(options);
}

bool render_point_in_sidebar(Vector2 point, const RenderOptions *options) {
    return point.x >= sidebar_left(options);
}

bool render_point_in_graph_canvas(Vector2 point, const RenderOptions *options) {
    return point.x < sidebar_left(options);
}

Color render_background_color(const RenderOptions *options) {
    return theme_for_options(options).background;
}

// static unsigned char color_mix_channel(unsigned char from, unsigned char to,
//                                        float amount) {
//     return (unsigned char)((float)from + ((float)to - (float)from) * amount);
// }
//
// static Color color_mix(Color from, Color to, float amount) {
//     if (amount < 0.0f) amount = 0.0f;
//     if (amount > 1.0f) amount = 1.0f;
//
//     return (Color){
//         color_mix_channel(from.r, to.r, amount),
//         color_mix_channel(from.g, to.g, amount),
//         color_mix_channel(from.b, to.b, amount),
//         color_mix_channel(from.a, to.a, amount),
//     };
// }

static bool color_is_dark(Color color) {
    int luminance = (int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114;
    return luminance < 140000;
}

static Color node_text_color(Color fill) {
    return color_is_dark(fill) ? RAYWHITE : (Color){0, 0, 0, 255};
}

static Color node_fill_color(const Theme *theme, NodeColor color) {
    switch (color) {
    case NODE_WHITE:
        return theme->node_white;
    case NODE_GRAY:
        return theme->node_gray;
    case NODE_BLACK:
        return theme->node_black;
    default:
        return theme->node_white;
    }
}

static Color tree_node_fill_color(const Theme *theme, const Node *node) {
    if (node->color == NODE_WHITE) return theme->node_white;

    return theme->node_black;
}

/*
 * BFS uses discrete level bands instead of a stretched gradient. The colors are
 * anchored near DFS's finished-node color so the final state feels visually
 * related across algorithms.
 */
static Color bfs_level_fill_color(const Theme *theme, const Node *node) {
    if (node->level < 0) return theme->node_white;

    // Color even_level = color_mix(theme->node_black, theme->edge_forward, 0.34f);
    // Color odd_level = color_mix(theme->node_black, theme->node_gray, 0.38f);
    Color even_level = {2, 12, 25, 255};
    Color odd_level = {225, 118, 21, 255};

    return node->level % 2 == 0 ? even_level : odd_level;
}

static Color edge_color(const Theme *theme, EdgeType type) {
    switch (type) {
    case EDGE_TREE:
        return theme->edge_tree;
    case EDGE_BACK:
        return theme->edge_back;
    case EDGE_FORWARD:
        return theme->edge_forward;
    case EDGE_CROSS:
        return theme->edge_cross;
    case EDGE_UNCLASSIFIED:
    default:
        return theme->edge_unclassified;
    }
}

static Color edge_render_color(const Theme *theme, EdgeType type,
                               const RenderOptions *options) {
    if (options->algorithm_mode == ALGORITHM_BFS) return theme->edge_unclassified;
    return edge_color(theme, type);
}

static int max_bfs_level(const Graph *graph) {
    int max_level = 0;

    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].level > max_level) max_level = graph->nodes[i].level;
    }

    return max_level;
}

static int node_count_with_level(const Graph *graph) {
    int count = 0;

    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].level >= 0) count++;
    }

    return count;
}

static int settled_node_count(const Graph *graph) {
    int count = 0;

    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].color == NODE_BLACK) count++;
    }

    return count;
}

static int dijkstra_source_node(const Trace *trace) {
    for (size_t i = 0; i < trace->count; i++) {
        if (trace->events[i].type == TRACE_EVENT_SET_DISTANCE)
            return trace->events[i].node;
    }

    return -1;
}

static Vector2 vector_add(Vector2 a, Vector2 b) {
    return (Vector2){a.x + b.x, a.y + b.y};
}

static Vector2 vector_subtract(Vector2 a, Vector2 b) {
    return (Vector2){a.x - b.x, a.y - b.y};
}

static Vector2 vector_scale(Vector2 value, float scale) {
    return (Vector2){value.x * scale, value.y * scale};
}

static Vector2 vector_normalize(Vector2 value) {
    float length = sqrtf(value.x * value.x + value.y * value.y);

    if (length <= 0.0001f) return (Vector2){1.0f, 0.0f};

    return (Vector2){value.x / length, value.y / length};
}

static Vector2 quadratic_point(Vector2 start, Vector2 control, Vector2 end,
                               float t) {
    float inv = 1.0f - t;
    return (Vector2){
        inv * inv * start.x + 2.0f * inv * t * control.x + t * t * end.x,
        inv * inv * start.y + 2.0f * inv * t * control.y + t * t * end.y,
    };
}

#define GRAPHE_ARROW_LENGTH 15.0f
#define GRAPHE_ARROW_WIDTH 11.0f
#define GRAPHE_ARROW_BODY_OVERLAP 0.5f

static Vector2 edge_body_arrow_endpoint(Vector2 tip, Vector2 direction) {
    return vector_subtract(
        tip,
        vector_scale(direction, GRAPHE_ARROW_LENGTH - GRAPHE_ARROW_BODY_OVERLAP));
}

static void draw_arrowhead_shape(Vector2 tip, Vector2 direction, float length,
                                 float width, Color color) {
    Vector2 normal = {-direction.y, direction.x};
    Vector2 base = vector_subtract(tip, vector_scale(direction, length));
    Vector2 left = vector_add(base, vector_scale(normal, width * 0.5f));
    Vector2 right = vector_subtract(base, vector_scale(normal, width * 0.5f));

    DrawTriangle(tip, right, left, color);
}

static void draw_arrowhead(Vector2 tip, Vector2 direction, Color color) {
    draw_arrowhead_shape(tip, direction, GRAPHE_ARROW_LENGTH, GRAPHE_ARROW_WIDTH,
                         color);
}

#ifdef GRAPHE_GLOW
static void draw_arrowhead_glow(Vector2 tip, Vector2 direction, Color color) {
    const float lengths[] = {21.0f, 18.0f, 16.0f};
    const float widths[] = {18.0f, 15.0f, 12.5f};
    const unsigned char alphas[] = {14, 22, 32};

    for (int i = 0; i < 3; i++) {
        Color glow = color;
        glow.a = alphas[i];
        draw_arrowhead_shape(tip, direction, lengths[i], widths[i], glow);
    }
}
#endif

static void draw_text(const RenderResources *resources, const char *text,
                      Vector2 position, float size, Color color) {
    DrawTextEx(resources->font, text, position, size, size * 0.04f, color);
}

static Vector2 measure_text(const RenderResources *resources, const char *text,
                            float size) {
    return MeasureTextEx(resources->font, text, size, size * 0.04f);
}

void render_resources_load(RenderResources *resources) {
    const char *font_paths[] = {
        "assets/fonts/AtkinsonHyperlegible-Regular.ttf",
        "assets/fonts/Inter-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    resources->font = GetFontDefault();
    resources->custom_font_loaded = false;
    resources->graph_background_loaded = false;
    resources->graph_background_width = 0;
    resources->graph_background_height = 0;
    resources->graph_background_dark_mode = false;
    resources->graph_background_ui_scale = 0.0f;
    resources->edge_curve_cache = NULL;
    resources->edge_curve_cache_capacity = 0;

    for (size_t i = 0; i < sizeof(font_paths) / sizeof(font_paths[0]); i++) {
        if (!FileExists(font_paths[i])) continue;

        Font font = LoadFontEx(font_paths[i], 96, NULL, 0);
        if (font.texture.id == 0) continue;

        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        resources->font = font;
        resources->custom_font_loaded = true;
        return;
    }
}

static void unload_graph_background(RenderResources *resources) {
    if (resources->graph_background_loaded)
        UnloadRenderTexture(resources->graph_background);

    resources->graph_background_loaded = false;
    resources->graph_background_width = 0;
    resources->graph_background_height = 0;
}

void render_resources_unload(RenderResources *resources) {
    unload_graph_background(resources);
    free(resources->edge_curve_cache);
    resources->edge_curve_cache = NULL;
    resources->edge_curve_cache_capacity = 0;
    if (resources->custom_font_loaded) UnloadFont(resources->font);
    resources->custom_font_loaded = false;
}

/*
 * Clears cached edge curve choices when graph structure, trace classifications,
 * or automatic layout changes. Manual node dragging intentionally keeps the
 * cached side so curves do not flip while the user is editing positions.
 */
void render_clear_edge_curve_cache(RenderResources *resources) {
    if (resources == NULL || resources->edge_curve_cache == NULL) return;

    for (size_t i = 0; i < resources->edge_curve_cache_capacity; i++)
        resources->edge_curve_cache[i].valid = false;
}

static int event_matches_visible_edge(const Graph *graph, int event_edge,
                                      size_t edge_index) {
    (void)graph;

    if (event_edge < 0) return 0;
    return event_edge == (int)edge_index;
}

static int is_active_edge(const Graph *graph, const Trace *trace,
                          size_t active_index, size_t edge_index) {
    if (active_index >= trace->count) return 0;

    return event_matches_visible_edge(graph, trace->events[active_index].edge,
                                      edge_index);
}

static int is_active_examine_edge(const Graph *graph, const Trace *trace,
                                  size_t active_index, size_t edge_index) {
    if (active_index >= trace->count) return 0;
    if (trace->events[active_index].type != TRACE_EVENT_EXAMINE_EDGE) return 0;

    return event_matches_visible_edge(graph, trace->events[active_index].edge,
                                      edge_index);
}

static int is_active_node(const Trace *trace, size_t active_index,
                          size_t node_index) {
    if (active_index >= trace->count) return 0;

    return trace->events[active_index].node == (int)node_index;
}

static void draw_quadratic_edge(Vector2 start, Vector2 control, Vector2 end,
                                float thickness, Color color) {
    const int segments = 24;
    Vector2 previous = start;

    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        Vector2 current = quadratic_point(start, control, end, t);
        DrawLineEx(previous, current, thickness, color);
        previous = current;
    }
}

static void draw_edge_path(EdgeGeometry geometry, Vector2 end, float thickness,
                           Color color) {
    if (geometry.bend <= 0.0f) {
        DrawLineEx(geometry.start, end, thickness, color);
        return;
    }

    draw_quadratic_edge(geometry.start, geometry.control, end, thickness, color);
}

#ifdef GRAPHE_GLOW
static void draw_edge_glow(EdgeGeometry geometry, Vector2 end, Color color) {
    const float widths[] = {15.0f, 10.0f, 6.0f};
    const unsigned char alphas[] = {20, 32, 50};

    for (int i = 0; i < 3; i++) {
        Color glow = color;
        glow.a = alphas[i];
        draw_edge_path(geometry, end, widths[i], glow);
    }
}

static Vector2 edge_glow_arrow_endpoint(Vector2 tip, Vector2 direction) {
    return vector_subtract(tip, vector_scale(direction, GRAPHE_ARROW_LENGTH * 1.0));
}
#endif

static float clamp_float(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static float vector_length(Vector2 value) {
    return sqrtf(value.x * value.x + value.y * value.y);
}

static float vector_distance_squared(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static float edge_bend_amount(EdgeType type, float distance) {
    float factor;

    switch (type) {
    case EDGE_BACK:
        factor = 0.24f;
        break;
    case EDGE_FORWARD:
        factor = 0.18f;
        break;
    case EDGE_CROSS:
        factor = 0.22f;
        break;
    default:
        return 0.0f;
    }

    return clamp_float(distance * factor, 18.0f, 70.0f);
}

/*
 * Assigns stable curve lanes when multiple directed edges share the same two
 * endpoints. Reciprocal directions use the same local sign; because their edge
 * directions (and therefore normals) are reversed, they land on opposite sides
 * of the straight node-to-node line. Same-direction parallels alternate sides.
 */
static EdgeTopologyCurve edge_topology_curve(const Graph *graph, size_t edge_index,
                                             float distance) {
    EdgeTopologyCurve curve = {0};
    const Edge *edge = &graph->edges[edge_index];

    if (!graph->directed || edge->from == edge->to) return curve;

    int same_direction_count = 0;
    int same_direction_before = 0;
    int reverse_direction_count = 0;
    for (size_t i = 0; i < graph->edge_count; i++) {
        const Edge *other = &graph->edges[i];

        if (other->from == edge->from && other->to == edge->to) {
            same_direction_count++;
            if (i < edge_index) same_direction_before++;
        } else if (other->from == edge->to && other->to == edge->from) {
            reverse_direction_count++;
        }
    }

    float base_bend = clamp_float(distance * 0.16f, 24.0f, 58.0f);
    if (reverse_direction_count > 0) {
        curve.separated = true;
        curve.bend = clamp_float(base_bend + (float)same_direction_before * 13.0f,
                                 24.0f, 82.0f);
        curve.sign = 1.0f;
    } else if (same_direction_count > 1) {
        curve.separated = true;
        curve.bend = clamp_float(
            base_bend + (float)(same_direction_before / 2) * 13.0f, 24.0f, 82.0f);
        curve.sign = same_direction_before % 2 == 0 ? -1.0f : 1.0f;
    }

    return curve;
}

static float fallback_edge_curve_sign(const Node *from, const Node *to,
                                      EdgeType type, const RenderOptions *options) {
    Vector2 mid = {(from->x + to->x) * 0.5f, (from->y + to->y) * 0.5f};

    if (type == EDGE_BACK || type == EDGE_FORWARD) {
        float graph_center = render_graph_area_width(options) * 0.5f;
        return mid.x < graph_center ? -1.0f : 1.0f;
    }

    return from->x <= to->x ? 1.0f : -1.0f;
}

static EdgeGeometry curved_edge_geometry(const Node *from, const Node *to,
                                         EdgeGeometry geometry, Vector2 normal,
                                         float bend, float sign,
                                         float node_padding) {
    Vector2 start_center = {from->x, from->y};
    Vector2 end_center = {to->x, to->y};
    Vector2 mid = vector_scale(vector_add(geometry.start, geometry.end), 0.5f);

    geometry.control = vector_add(mid, vector_scale(normal, bend * sign));
    geometry.start =
        vector_add(start_center, vector_scale(vector_normalize(vector_subtract(
                                                  geometry.control, start_center)),
                                              GRAPHE_NODE_RADIUS));
    geometry.end = vector_add(
        end_center,
        vector_scale(vector_normalize(vector_subtract(geometry.control, end_center)),
                     GRAPHE_NODE_RADIUS + node_padding));
    geometry.end_direction =
        vector_normalize(vector_subtract(geometry.end, geometry.control));
    geometry.bend = bend;

    return geometry;
}

/*
 * Scores how much a candidate curve passes near other nodes. This is used only
 * when choosing a curve side; normal frames reuse the cached sign.
 */
static float edge_node_clutter_score(const Graph *graph, size_t edge_index,
                                     EdgeGeometry geometry) {
    const Edge *edge = &graph->edges[edge_index];
    const int sample_count = 9;
    const float near_radius = GRAPHE_NODE_RADIUS * 2.35f;
    const float overlap_radius = GRAPHE_NODE_RADIUS * 1.35f;
    const float near_radius_sq = near_radius * near_radius;
    const float overlap_radius_sq = overlap_radius * overlap_radius;
    float score = 0.0f;

    for (size_t node_index = 0; node_index < graph->node_count; node_index++) {
        if ((int)node_index == edge->from || (int)node_index == edge->to) continue;

        Vector2 node_center = {graph->nodes[node_index].x,
                               graph->nodes[node_index].y};
        float best_distance_sq = near_radius_sq;

        for (int sample = 1; sample <= sample_count; sample++) {
            float t = (float)sample / (float)(sample_count + 1);
            Vector2 point =
                quadratic_point(geometry.start, geometry.control, geometry.end, t);
            float distance_sq = vector_distance_squared(point, node_center);

            if (distance_sq < best_distance_sq) best_distance_sq = distance_sq;
        }

        if (best_distance_sq >= near_radius_sq) continue;

        score += (near_radius_sq - best_distance_sq) / near_radius_sq;
        if (best_distance_sq < overlap_radius_sq)
            score +=
                4.0f * (overlap_radius_sq - best_distance_sq) / overlap_radius_sq;
    }

    return score;
}

/*
 * Ensures the cache has a slot for each edge index. Cache entries are keyed by
 * endpoint and edge type so reused edge indices do not accidentally preserve a
 * stale curve side after graph or trace changes.
 */
static bool edge_curve_cache_reserve(RenderResources *resources,
                                     size_t required_capacity) {
    if (required_capacity <= resources->edge_curve_cache_capacity) return true;

    size_t capacity = resources->edge_curve_cache_capacity == 0
                          ? 64
                          : resources->edge_curve_cache_capacity;
    while (capacity < required_capacity) capacity *= 2;

    EdgeCurveCacheEntry *entries =
        realloc(resources->edge_curve_cache, capacity * sizeof(*entries));
    if (entries == NULL) return false;

    for (size_t i = resources->edge_curve_cache_capacity; i < capacity; i++)
        entries[i].valid = false;

    resources->edge_curve_cache = entries;
    resources->edge_curve_cache_capacity = capacity;
    return true;
}

static bool edge_curve_cache_get(const RenderResources *resources, size_t edge_index,
                                 const Edge *edge, float *sign) {
    if (edge_index >= resources->edge_curve_cache_capacity) return false;

    const EdgeCurveCacheEntry *entry = &resources->edge_curve_cache[edge_index];
    if (!entry->valid) return false;
    if (entry->from != edge->from || entry->to != edge->to ||
        entry->type != edge->type) {
        return false;
    }

    *sign = entry->sign;
    return true;
}

static void edge_curve_cache_set(RenderResources *resources, size_t edge_index,
                                 const Edge *edge, float sign) {
    if (!edge_curve_cache_reserve(resources, edge_index + 1)) return;

    EdgeCurveCacheEntry *entry = &resources->edge_curve_cache[edge_index];
    entry->valid = true;
    entry->from = edge->from;
    entry->to = edge->to;
    entry->type = edge->type;
    entry->sign = sign;
}

/*
 * Chooses which side of the straight edge the curve should bend toward. Both
 * sides are scored against nearby nodes once, then the chosen sign is cached for
 * stable, cheap rendering.
 */
static float edge_curve_sign(RenderResources *resources, const Graph *graph,
                             size_t edge_index, EdgeGeometry base_geometry,
                             Vector2 normal, float bend, float node_padding,
                             const RenderOptions *options) {
    const Edge *edge = &graph->edges[edge_index];
    const Node *from = &graph->nodes[edge->from];
    const Node *to = &graph->nodes[edge->to];
    float sign;

    if (edge_curve_cache_get(resources, edge_index, edge, &sign)) return sign;

    EdgeGeometry positive = curved_edge_geometry(from, to, base_geometry, normal,
                                                 bend, 1.0f, node_padding);
    EdgeGeometry negative = curved_edge_geometry(from, to, base_geometry, normal,
                                                 bend, -1.0f, node_padding);
    float positive_score = edge_node_clutter_score(graph, edge_index, positive);
    float negative_score = edge_node_clutter_score(graph, edge_index, negative);
    float score_delta = positive_score - negative_score;

    if (fabsf(score_delta) > 0.05f) {
        sign = score_delta < 0.0f ? 1.0f : -1.0f;
    } else {
        sign = fallback_edge_curve_sign(from, to, edge->type, options);
    }

    edge_curve_cache_set(resources, edge_index, edge, sign);
    return sign;
}

/*
 * Computes all geometry needed by both the edge body and arrowhead. Curved
 * edges adjust their start/end points so the curve still meets node boundaries.
 */
static EdgeGeometry make_edge_geometry(RenderResources *resources,
                                       const Graph *graph, size_t edge_index,
                                       int active, const RenderOptions *options) {
    const Edge *edge = &graph->edges[edge_index];
    const Node *from = &graph->nodes[edge->from];
    const Node *to = &graph->nodes[edge->to];
    Vector2 start_center = {from->x, from->y};
    Vector2 end_center = {to->x, to->y};
    Vector2 delta = vector_subtract(end_center, start_center);
    float distance = vector_length(delta);
    Vector2 direction = vector_normalize(delta);
    float node_padding = graph->directed ? 2.0f : 0.0f;
    EdgeGeometry geometry = {
        .start =
            vector_add(start_center, vector_scale(direction, GRAPHE_NODE_RADIUS)),
        .control = {0.0f, 0.0f},
        .end = vector_subtract(
            end_center, vector_scale(direction, GRAPHE_NODE_RADIUS + node_padding)),
        .end_direction = direction,
        .bend = 0.0f,
    };
    EdgeTopologyCurve topology = edge_topology_curve(graph, edge_index, distance);
    float bend = topology.separated ? topology.bend
                 : active && edge->type == EDGE_UNCLASSIFIED
                     ? 0.0f
                     : edge_bend_amount(edge->type, distance);

    if (bend <= 0.0f) return geometry;

    Vector2 normal = {-direction.y, direction.x};
    float sign = topology.separated
                     ? topology.sign
                     : edge_curve_sign(resources, graph, edge_index, geometry,
                                       normal, bend, node_padding, options);

    return curved_edge_geometry(from, to, geometry, normal, bend, sign,
                                node_padding);
}

static void draw_edge_body(const Graph *graph, size_t edge_index,
                           EdgeGeometry geometry, int active_examine,
                           const Theme *theme, const RenderOptions *options) {
    Color color =
        active_examine
            ? theme->active
            : edge_render_color(theme, graph->edges[edge_index].type, options);
    Vector2 end = graph->directed ? edge_body_arrow_endpoint(geometry.end,
                                                             geometry.end_direction)
                                  : geometry.end;
#ifdef GRAPHE_GLOW
    Vector2 glow_end = graph->directed ? edge_glow_arrow_endpoint(
                                             geometry.end, geometry.end_direction)
                                       : end;

    if (active_examine) draw_edge_glow(geometry, glow_end, theme->active);
#endif
    draw_edge_path(geometry, end, 2.5f, color);
}

static void draw_edge_arrow(const Graph *graph, size_t edge_index,
                            EdgeGeometry geometry, int active_examine,
                            const Theme *theme, const RenderOptions *options) {
    Color color =
        active_examine
            ? theme->active
            : edge_render_color(theme, graph->edges[edge_index].type, options);

#ifdef GRAPHE_GLOW
    if (active_examine)
        draw_arrowhead_glow(geometry.end, geometry.end_direction, theme->active);
#endif
    draw_arrowhead(geometry.end, geometry.end_direction, color);
}

static void draw_edge_weight(const RenderResources *resources, const Edge *edge,
                             EdgeGeometry geometry, const Theme *theme) {
    char weight[24];
    snprintf(weight, sizeof(weight), "%d", edge->weight);

    Vector2 point =
        geometry.bend > 0.0f
            ? quadratic_point(geometry.start, geometry.control, geometry.end, 0.5f)
            : vector_scale(vector_add(geometry.start, geometry.end), 0.5f);
    Vector2 direction =
        vector_normalize(vector_subtract(geometry.end, geometry.start));
    Vector2 normal = {-direction.y, direction.x};
    point = vector_add(point, vector_scale(normal, 10.0f));

    const float font_size = 16.0f;
    Vector2 size = measure_text(resources, weight, font_size);
    Rectangle box = {point.x - size.x * 0.5f - 5.0f, point.y - size.y * 0.5f - 2.0f,
                     size.x + 10.0f, size.y + 4.0f};
    const float border = 1.5f;
    Rectangle fill = {box.x + border, box.y + border, box.width - border * 2.0f,
                      box.height - border * 2.0f};
    DrawRectangleRounded(box, 0.35f, 6, theme->panel_border);
    DrawRectangleRounded(fill, 0.31f, 6, theme->panel);
    draw_text(resources, weight,
              (Vector2){point.x - size.x * 0.5f, point.y - size.y * 0.5f}, font_size,
              theme->text);
}

static void draw_time_label(const RenderResources *resources, const Node *node,
                            Color color) {
    char times[64];
    char discover[16];
    char finish[16];

    if (node->discover_time >= 0) {
        snprintf(discover, sizeof(discover), "%d", node->discover_time);
    } else {
        snprintf(discover, sizeof(discover), "-");
    }

    if (node->finish_time >= 0) {
        snprintf(finish, sizeof(finish), "%d", node->finish_time);
    } else {
        snprintf(finish, sizeof(finish), "-");
    }

    snprintf(times, sizeof(times), "%s %s", discover, finish);
    Vector2 size = measure_text(resources, times, 15.0f);
    draw_text(resources, times, (Vector2){node->x - size.x * 0.5f, node->y + 7.0f},
              15.0f, color);
}

static void draw_level_label(const RenderResources *resources, const Node *node,
                             Color color) {
    if (node->level < 0) return;

    char level[16];
    snprintf(level, sizeof(level), "L%d", node->level);
    Vector2 size = measure_text(resources, level, 15.0f);
    draw_text(resources, level, (Vector2){node->x - size.x * 0.5f, node->y + 7.0f},
              15.0f, color);
}

static void draw_distance_label(const RenderResources *resources, const Node *node,
                                Color color) {
    char distance[24];
    if (node->distance == GRAPHE_DISTANCE_INFINITY) {
        snprintf(distance, sizeof(distance), "d=inf");
    } else {
        snprintf(distance, sizeof(distance), "d=%lld", node->distance);
    }

    Vector2 size = measure_text(resources, distance, 15.0f);
    draw_text(resources, distance,
              (Vector2){node->x - size.x * 0.5f, node->y + 7.0f}, 15.0f, color);
}

#ifdef GRAPHE_GLOW
static void draw_node_glow(Vector2 center, Color color) {
    const float inner_radii[] = {
        GRAPHE_NODE_RADIUS + 1.0f,
        GRAPHE_NODE_RADIUS,
        GRAPHE_NODE_RADIUS,
        GRAPHE_NODE_RADIUS,
    };
    const float outer_radii[] = {
        GRAPHE_NODE_RADIUS + 9.0f,
        GRAPHE_NODE_RADIUS + 6.0f,
        GRAPHE_NODE_RADIUS + 4.0f,
        GRAPHE_NODE_RADIUS + 2.0f,
    };
    const unsigned char alphas[] = {18, 25, 37, 52};

    for (int i = 0; i < 4; i++) {
        Color glow = color;
        glow.a = alphas[i];
        DrawRing(center, inner_radii[i], outer_radii[i], 0, 360, 64, glow);
    }
}
#endif

/*
 * Draws node fill, label, and mode-specific secondary text. The color selection
 * lives here because DFS, BFS, Dijkstra, and tree mode intentionally emphasize
 * different state on the same Graph data.
 */
static void draw_node(const RenderResources *resources, const Node *node, int active,
                      const Theme *theme, const RenderOptions *options) {
    (void)active;
    Vector2 center = {node->x, node->y};
    bool show_times = options->algorithm_mode == ALGORITHM_DFS;
    bool show_level = options->algorithm_mode == ALGORITHM_BFS;
    bool show_distance = options->algorithm_mode == ALGORITHM_DIJKSTRA;
    bool show_tree_output = options->algorithm_mode == ALGORITHM_TREE;
    Color fill = show_level         ? bfs_level_fill_color(theme, node)
                 : show_tree_output ? tree_node_fill_color(theme, node)
                                    : node_fill_color(theme, node->color);
    Color outline = theme->node_outline;
    Color label_color = show_level || show_tree_output ? node_text_color(fill)
                        : node->color == NODE_BLACK    ? RAYWHITE
                                                       : (Color){0, 0, 0, 255};
    Color time_color = label_color;
    Vector2 label_size = measure_text(resources, node->label, 24.0f);

    time_color.a = 220;

#ifdef GRAPHE_GLOW
    if (active) draw_node_glow(center, theme->active);
#endif
    DrawCircleV(center, GRAPHE_NODE_RADIUS, fill);
    DrawRing(center, GRAPHE_NODE_RADIUS - 1.8f, GRAPHE_NODE_RADIUS, 0, 360, 64,
             outline);

    float label_y = show_times || show_level || show_distance
                        ? node->y - label_size.y + 3.0f
                        : node->y - label_size.y * 0.5f;

    draw_text(resources, node->label,
              (Vector2){node->x - label_size.x * 0.5f, label_y}, 24.0f, label_color);
    if (show_times) draw_time_label(resources, node, time_color);
    if (show_level) draw_level_label(resources, node, time_color);
    if (show_distance) draw_distance_label(resources, node, time_color);
}

static const char *event_type_name(TraceEventType type) {
    switch (type) {
    case TRACE_EVENT_DISCOVER_NODE:
        return "discover node";
    case TRACE_EVENT_EXAMINE_EDGE:
        return "examine edge";
    case TRACE_EVENT_CLASSIFY_EDGE:
        return "classify edge";
    case TRACE_EVENT_FINISH_NODE:
        return "finish node";
    case TRACE_EVENT_SET_DISTANCE:
        return "set distance";
    case TRACE_EVENT_RELAX_EDGE:
        return "relax edge";
    case TRACE_EVENT_SETTLE_NODE:
        return "settle node";
    default:
        return "unknown";
    }
}

static const char *layout_mode_name(LayoutMode mode) {
    switch (mode) {
    case LAYOUT_TRACE_FOREST:
        return "Traversal forest";
    case LAYOUT_CIRCULAR:
        return "Circular";
    case LAYOUT_MANUAL:
        return "Manual";
    default:
        return "Unknown";
    }
}

static bool append_tree_text(char *buffer, size_t buffer_size, size_t *used,
                             const char *text, bool separated) {
    int written;

    if (separated && *used > 0 && buffer[*used - 1] != '(' &&
        buffer[*used - 1] != ' ') {
        if (*used >= buffer_size - 1) return false;
        buffer[*used] = ' ';
        (*used)++;
        buffer[*used] = '\0';
    }
    if (*used >= buffer_size - 1) return false;

    written = snprintf(buffer + *used, buffer_size - *used, "%s", text);
    if (written < 0) return false;
    if ((size_t)written >= buffer_size - *used) {
        buffer[buffer_size - 1] = '\0';
        *used = buffer_size - 1;
        return false;
    }

    *used += (size_t)written;
    return true;
}

static void append_parenthesized_inorder_node(const Graph *graph, int node,
                                              size_t depth, char *buffer,
                                              size_t buffer_size, size_t *used) {
    bool has_children;
    bool emitted_node = false;
    int child_index = 0;

    if (node < 0 || (size_t)node >= graph->node_count) return;
    if (depth > graph->node_count) return;

    has_children = graph->first_out[node] != -1;
    if (has_children && !append_tree_text(buffer, buffer_size, used, "(", true))
        return;

    for (int edge_id = graph->first_out[node]; edge_id != -1;
         edge_id = graph_next_adjacent_edge(graph, edge_id, node, false)) {
        int child = graph_edge_neighbor(graph, edge_id, node);

        if (child < 0) continue;
        if (child_index == 1) {
            if (!append_tree_text(buffer, buffer_size, used,
                                  graph->nodes[node].label, true))
                return;
            emitted_node = true;
        }
        child_index++;
        append_parenthesized_inorder_node(graph, child, depth + 1, buffer,
                                          buffer_size, used);
    }

    if (!emitted_node)
        append_tree_text(buffer, buffer_size, used, graph->nodes[node].label, true);
    if (has_children) append_tree_text(buffer, buffer_size, used, ")", false);
}

static int tree_expression_root(const Graph *graph) {
    for (size_t node = 0; node < graph->node_count; node++) {
        bool has_parent = false;

        for (size_t edge = 0; edge < graph->edge_count; edge++) {
            if (graph->edges[edge].to == (int)node) has_parent = true;
        }
        if (!has_parent) return (int)node;
    }

    return graph->node_count > 0 ? 0 : -1;
}

static void build_tree_output(const Graph *graph, const Trace *trace,
                              size_t active_index, TreeTraversalOrder order,
                              char *buffer, size_t buffer_size) {
    if (buffer_size == 0) return;
    buffer[0] = '\0';

    size_t used = 0;
    if (order == TREE_ORDER_INORDER && trace->count > 0 &&
        active_index >= trace->count - 1) {
        append_parenthesized_inorder_node(graph, tree_expression_root(graph), 0,
                                          buffer, buffer_size, &used);
        return;
    }

    for (size_t i = 0; i <= active_index && i < trace->count; i++) {
        const TraceEvent *event = &trace->events[i];
        if (event->type != TRACE_EVENT_DISCOVER_NODE) continue;
        if (!append_tree_text(buffer, buffer_size, &used,
                              graph->nodes[event->node].label, true))
            return;
    }
}

static void format_distance(long long distance, char *buffer, size_t buffer_size) {
    if (distance == GRAPHE_DISTANCE_INFINITY) {
        snprintf(buffer, buffer_size, "inf");
    } else {
        snprintf(buffer, buffer_size, "%lld", distance);
    }
}

static void describe_event(const Graph *graph, const Trace *trace,
                           size_t active_index, const RenderOptions *options,
                           char *buffer, size_t buffer_size) {
    if (active_index >= trace->count) {
        snprintf(buffer, buffer_size, "No event selected");
        return;
    }

    const TraceEvent *event = &trace->events[active_index];

    if (options->algorithm_mode == ALGORITHM_TREE) {
        char output[128];
        build_tree_output(graph, trace, active_index, options->tree_order, output,
                          sizeof(output));
        snprintf(buffer, buffer_size, "%s: %s",
                 tree_traversal_order_name(options->tree_order), output);
        return;
    }

    if (options->algorithm_mode == ALGORITHM_DIJKSTRA) {
        switch (event->type) {
        case TRACE_EVENT_SET_DISTANCE:
            snprintf(buffer, buffer_size, "set source %s distance to 0",
                     graph->nodes[event->node].label);
            break;
        case TRACE_EVENT_SETTLE_NODE:
            snprintf(buffer, buffer_size, "settle %s at distance %lld",
                     graph->nodes[event->node].label, event->distance);
            break;
        case TRACE_EVENT_EXAMINE_EDGE: {
            const Edge *edge = &graph->edges[event->edge];
            long long from_distance = graph->nodes[event->from].distance;
            long long candidate = GRAPHE_DISTANCE_INFINITY;
            if (from_distance != GRAPHE_DISTANCE_INFINITY && edge->weight >= 0 &&
                (long long)edge->weight <= GRAPHE_DISTANCE_INFINITY - from_distance)
                candidate = from_distance + edge->weight;

            char candidate_text[24];
            char current_text[24];
            format_distance(candidate, candidate_text, sizeof(candidate_text));
            format_distance(graph->nodes[event->to].distance, current_text,
                            sizeof(current_text));
            long long current = graph->nodes[event->to].distance;
            const char *result =
                candidate == GRAPHE_DISTANCE_INFINITY
                    ? "; distance exceeds supported range"
                : current != GRAPHE_DISTANCE_INFINITY && candidate >= current
                    ? "; no improvement"
                    : "";
            snprintf(buffer, buffer_size,
                     "consider %s %s %s (w=%d): candidate %s, current %s%s",
                     graph->nodes[event->from].label, graph->directed ? "->" : "-",
                     graph->nodes[event->to].label, edge->weight, candidate_text,
                     current_text, result);
            break;
        }
        case TRACE_EVENT_RELAX_EDGE: {
            char old_distance[24];
            format_distance(event->old_distance, old_distance, sizeof(old_distance));
            snprintf(buffer, buffer_size, "relax %s %s %s: %s -> %lld",
                     graph->nodes[event->from].label, graph->directed ? "->" : "-",
                     graph->nodes[event->to].label, old_distance, event->distance);
            break;
        }
        default:
            snprintf(buffer, buffer_size, "Unknown Dijkstra event");
            break;
        }
        return;
    }

    if (options->algorithm_mode == ALGORITHM_BFS) {
        switch (event->type) {
        case TRACE_EVENT_DISCOVER_NODE:
            snprintf(buffer, buffer_size, "discover %s at level %d",
                     graph->nodes[event->node].label, event->time);
            break;
        case TRACE_EVENT_FINISH_NODE: {
            int level = graph->nodes[event->node].level;
            if (level >= 0) {
                snprintf(buffer, buffer_size, "finish %s at level %d",
                         graph->nodes[event->node].label, level);
            } else {
                snprintf(buffer, buffer_size, "finish %s",
                         graph->nodes[event->node].label);
            }
            break;
        }
        case TRACE_EVENT_EXAMINE_EDGE: {
            int level = graph->nodes[event->from].level;
            snprintf(buffer, buffer_size, "scan %s %s %s from level %d",
                     graph->nodes[event->from].label, graph->directed ? "->" : "-",
                     graph->nodes[event->to].label, level);
            break;
        }
        case TRACE_EVENT_CLASSIFY_EDGE:
            if (event->edge_type == EDGE_TREE) {
                snprintf(buffer, buffer_size, "enqueue %s at level %d",
                         graph->nodes[event->to].label, event->time);
            } else {
                snprintf(buffer, buffer_size, "%s already reached at level %d",
                         graph->nodes[event->to].label, event->time);
            }
            break;
        default:
            snprintf(buffer, buffer_size, "Unknown BFS event");
            break;
        }
        return;
    }

    switch (event->type) {
    case TRACE_EVENT_DISCOVER_NODE:
    case TRACE_EVENT_FINISH_NODE:
        snprintf(buffer, buffer_size, "%s %s at t=%d", event_type_name(event->type),
                 graph->nodes[event->node].label, event->time);
        break;
    case TRACE_EVENT_EXAMINE_EDGE: {
        snprintf(buffer, buffer_size, "examine %s %s %s",
                 graph->nodes[event->from].label, graph->directed ? "->" : "-",
                 graph->nodes[event->to].label);
        break;
    }
    case TRACE_EVENT_CLASSIFY_EDGE: {
        snprintf(buffer, buffer_size, "%s %s %s is %s",
                 graph->nodes[event->from].label, graph->directed ? "->" : "-",
                 graph->nodes[event->to].label, edge_type_name(event->edge_type));
        break;
    }
    default:
        snprintf(buffer, buffer_size, "Unknown event");
        break;
    }
}

static int gui_color(Color color) {
    return ((int)color.r << 24) | ((int)color.g << 16) | ((int)color.b << 8) |
           (int)color.a;
}

static Color lighten_color(Color color, float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;

    color.r = (unsigned char)((float)color.r + (255.0f - (float)color.r) * amount);
    color.g = (unsigned char)((float)color.g + (255.0f - (float)color.g) * amount);
    color.b = (unsigned char)((float)color.b + (255.0f - (float)color.b) * amount);
    return color;
}

static Color with_alpha(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}

static void apply_gui_style(const RenderResources *resources,
                            const RenderOptions *options, const Theme *theme) {
    int text_size = (int)(ui_size(options, 15.0f) + 0.5f);
    int padding = (int)(ui_size(options, 14.0f) + 0.5f);
    Color button_hover = lighten_color(theme->button, 0.12f);
    Color button_active_hover = lighten_color(theme->button_active, 0.10f);

    GuiSetFont(resources->font);
    GuiSetStyle(DEFAULT, TEXT_SIZE, text_size);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
    GuiSetStyle(DEFAULT, TEXT_LINE_SPACING, (int)(ui_size(options, 20.0f) + 0.5f));
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL, TEXT_ALIGN_MIDDLE);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, gui_color(theme->panel_border));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, gui_color(theme->button));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, gui_color(theme->text));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, gui_color(theme->panel_border));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, gui_color(button_hover));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, gui_color(theme->text));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, gui_color(theme->panel_border));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, gui_color(button_active_hover));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, gui_color(theme->text));
    GuiSetStyle(DEFAULT, LINE_COLOR, gui_color(theme->panel_border));
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, gui_color(theme->panel));
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiSetStyle(TOGGLE, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(TOGGLE, TEXT_PADDING, padding);
    GuiSetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(TEXTBOX, TEXT_PADDING, padding);
}

static bool rounded_button(const RenderResources *resources,
                           const RenderOptions *options, const Theme *theme,
                           Rectangle bounds, const char *text, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color base = active ? theme->button_active : theme->button;
    Color fill = hovered ? lighten_color(base, 0.10f) : base;
    Color border =
        active ? lighten_color(theme->button_active, 0.18f) : theme->panel_border;
    float text_size = ui_size(options, 17.0f);
    Vector2 text_size_px = measure_text(resources, text, text_size);

    DrawRectangleRounded(bounds, 0.22f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.22f, 8, border);
    draw_text(resources, text,
              (Vector2){bounds.x + (bounds.width - text_size_px.x) * 0.5f,
                        bounds.y + (bounds.height - text_size_px.y) * 0.5f -
                            ui_size(options, 1.0f)},
              text_size, theme->text);

    return hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

static bool rounded_choice_button(const RenderResources *resources,
                                  const RenderOptions *options, const Theme *theme,
                                  Rectangle bounds, const char *text, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color base = active ? theme->button_active : theme->button;
    Color fill = hovered ? lighten_color(base, 0.10f) : base;
    Color border =
        active ? lighten_color(theme->button_active, 0.16f) : theme->panel_border;

    DrawRectangleRounded(bounds, 0.14f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.14f, 8, border);
    draw_text(resources, text,
              (Vector2){bounds.x + ui_size(options, 16.0f),
                        bounds.y + ui_size(options, 8.0f)},
              ui_size(options, 17.0f), theme->text);

    return hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

static Rectangle sidebar_rect(const RenderOptions *options, float x, float y,
                              float width, float height) {
    return (Rectangle){sidebar_left(options) + ui_size(options, x),
                       ui_size(options, y), ui_size(options, width),
                       ui_size(options, height)};
}

static Rectangle settings_button_rect(const RenderOptions *options) {
    float sidebar_width = render_sidebar_width(options) / ui_pixel_scale(options);
    return sidebar_rect(options, sidebar_width - 130.0f, 28.0f, 102.0f, 38.0f);
}

RenderUiResult render_update_options(RenderOptions *options) {
    RenderUiResult result = {0};

    if (options->graph_path_editing) return result;

    if (IsKeyPressed(KEY_TAB)) options->settings_open = !options->settings_open;
    if (IsKeyPressed(KEY_T)) options->dark_mode = !options->dark_mode;

    if (IsKeyPressed(KEY_A)) {
        options->alphabetical_order = !options->alphabetical_order;
        result.trace_changed = true;
    }

    if (IsKeyPressed(KEY_L)) {
        options->layout_mode = (options->layout_mode + 1) % 3;
        result.layout_changed = true;
    }

    if (IsKeyPressed(KEY_M)) {
        options->algorithm_mode =
            (options->algorithm_mode + 1) % ALGORITHM_MODE_COUNT;
        result.trace_changed = true;
    }

    if (IsKeyPressed(KEY_O)) {
        options->tree_order = (options->tree_order + 1) % 3;
        if (options->algorithm_mode == ALGORITHM_TREE) result.trace_changed = true;
    }

    return result;
}

static void merge_ui_result(RenderUiResult *result, RenderUiResult update) {
    result->consumed_click = result->consumed_click || update.consumed_click;
    result->layout_changed = result->layout_changed || update.layout_changed;
    result->trace_changed = result->trace_changed || update.trace_changed;
    result->graph_load_requested =
        result->graph_load_requested || update.graph_load_requested;
}

static void ensure_graph_word_background(RenderResources *resources,
                                         const RenderOptions *options,
                                         const Theme *theme, int width, int height);
static void draw_graph_word_background(const RenderResources *resources);

static void draw_settings_section(const RenderResources *resources,
                                  const RenderOptions *options, const Theme *theme,
                                  Rectangle bounds, const char *title) {
    DrawRectangleRounded(bounds, 0.04f, 8, theme->panel);
    DrawRectangleRoundedLines(bounds, 0.04f, 8, theme->panel_border);
    draw_text(resources, title,
              (Vector2){bounds.x + ui_size(options, 20.0f),
                        bounds.y + ui_size(options, 17.0f)},
              ui_size(options, 21.0f), theme->text);
}

static Rectangle settings_section_row(const RenderOptions *options,
                                      Rectangle section, int row) {
    float pad = ui_size(options, 20.0f);
    float row_height = ui_size(options, 32.0f);
    float row_gap = ui_size(options, 8.0f);

    return (Rectangle){
        section.x + pad,
        section.y + ui_size(options, 52.0f) + (row_height + row_gap) * (float)row,
        section.width - pad * 2.0f, row_height};
}

static RenderUiResult draw_settings(RenderResources *resources,
                                    RenderOptions *options, const Theme *theme) {
    RenderUiResult result = {0};
    float screen_width = (float)GetScreenWidth();
    float margin = ui_size(options, 48.0f);
    float max_content_width = ui_size(options, 1040.0f);
    float content_width = screen_width - margin * 2.0f;

    if (content_width > max_content_width) content_width = max_content_width;

    float content_x = (screen_width - content_width) * 0.5f;
    float top = ui_size(options, 96.0f);
    float column_gap = ui_size(options, 24.0f);
    float column_width = (content_width - column_gap) * 0.5f;
    Rectangle algorithms = {content_x, top, column_width, ui_size(options, 218.0f)};
    Rectangle layouts = {content_x, algorithms.y + algorithms.height + column_gap,
                         column_width, ui_size(options, 166.0f)};
    Rectangle utilities = {content_x + column_width + column_gap, top, column_width,
                           ui_size(options, 238.0f)};
    Rectangle graph_options = {utilities.x,
                               utilities.y + utilities.height + column_gap,
                               column_width, ui_size(options, 144.0f)};
    Rectangle close_button = {screen_width - margin - ui_size(options, 104.0f),
                              ui_size(options, 29.0f), ui_size(options, 104.0f),
                              ui_size(options, 38.0f)};

    ensure_graph_word_background(resources, options, theme, GetScreenWidth(),
                                 GetScreenHeight());
    draw_graph_word_background(resources);

    draw_text(resources, "Settings", (Vector2){margin, ui_size(options, 33.0f)},
              ui_size(options, 30.0f), theme->text);

    if (rounded_button(resources, options, theme, close_button, "Close", false)) {
        options->settings_open = false;
        result.consumed_click = true;
    }

    draw_settings_section(resources, options, theme, algorithms, "Algorithm");
    if (rounded_choice_button(resources, options, theme,
                              settings_section_row(options, algorithms, 0),
                              "DFS: times and edge classes",
                              options->algorithm_mode == ALGORITHM_DFS) &&
        options->algorithm_mode != ALGORITHM_DFS) {
        options->algorithm_mode = ALGORITHM_DFS;
        result.trace_changed = true;
        result.consumed_click = true;
    }
    if (rounded_choice_button(
            resources, options, theme, settings_section_row(options, algorithms, 1),
            "BFS: levels and depth", options->algorithm_mode == ALGORITHM_BFS) &&
        options->algorithm_mode != ALGORITHM_BFS) {
        options->algorithm_mode = ALGORITHM_BFS;
        result.trace_changed = true;
        result.consumed_click = true;
    }
    if (rounded_choice_button(resources, options, theme,
                              settings_section_row(options, algorithms, 2),
                              "Dijkstra: weights and distances",
                              options->algorithm_mode == ALGORITHM_DIJKSTRA) &&
        options->algorithm_mode != ALGORITHM_DIJKSTRA) {
        options->algorithm_mode = ALGORITHM_DIJKSTRA;
        result.trace_changed = true;
        result.consumed_click = true;
    }
    if (rounded_choice_button(resources, options, theme,
                              settings_section_row(options, algorithms, 3),
                              "Tree traversal: expression output",
                              options->algorithm_mode == ALGORITHM_TREE) &&
        options->algorithm_mode != ALGORITHM_TREE) {
        options->algorithm_mode = ALGORITHM_TREE;
        result.trace_changed = true;
        result.consumed_click = true;
    }

    draw_settings_section(resources, options, theme, layouts, "Layout");
    if (rounded_choice_button(
            resources, options, theme, settings_section_row(options, layouts, 0),
            "Traversal forest", options->layout_mode == LAYOUT_TRACE_FOREST) &&
        options->layout_mode != LAYOUT_TRACE_FOREST) {
        options->layout_mode = LAYOUT_TRACE_FOREST;
        result.layout_changed = true;
        result.consumed_click = true;
    }
    if (rounded_choice_button(
            resources, options, theme, settings_section_row(options, layouts, 1),
            "Circular layout", options->layout_mode == LAYOUT_CIRCULAR) &&
        options->layout_mode != LAYOUT_CIRCULAR) {
        options->layout_mode = LAYOUT_CIRCULAR;
        result.layout_changed = true;
        result.consumed_click = true;
    }
    draw_text(resources, "Dragging a node switches to manual layout.",
              (Vector2){layouts.x + ui_size(options, 20.0f),
                        layouts.y + ui_size(options, 136.0f)},
              ui_size(options, 16.0f), theme->muted_text);

    draw_settings_section(resources, options, theme, utilities, "Utilities");
    if (rounded_choice_button(
            resources, options, theme, settings_section_row(options, utilities, 0),
            options->dark_mode ? "Dark mode: on" : "Dark mode: off",
            options->dark_mode)) {
        options->dark_mode = !options->dark_mode;
        result.consumed_click = true;
    }

    Rectangle scale_row = settings_section_row(options, utilities, 1);
    char scale_text[64];
    snprintf(scale_text, sizeof(scale_text), "UI scale: %.0f%%",
             ui_scale_value(options) * 100.0f);
    draw_text(resources, scale_text,
              (Vector2){scale_row.x + ui_size(options, 16.0f),
                        scale_row.y + ui_size(options, 8.0f)},
              ui_size(options, 16.0f), theme->text);

    Rectangle minus_button = {
        scale_row.x + scale_row.width - ui_size(options, 104.0f), scale_row.y,
        ui_size(options, 46.0f), scale_row.height};
    Rectangle plus_button = {scale_row.x + scale_row.width - ui_size(options, 46.0f),
                             scale_row.y, ui_size(options, 46.0f), scale_row.height};
    if (rounded_button(resources, options, theme, minus_button, "-", false)) {
        if (change_ui_scale(options, -0.08f)) result.layout_changed = true;
        result.consumed_click = true;
    }
    if (rounded_button(resources, options, theme, plus_button, "+", false)) {
        if (change_ui_scale(options, 0.08f)) result.layout_changed = true;
        result.consumed_click = true;
    }

    Rectangle import_label = settings_section_row(options, utilities, 2);
    draw_text(resources, "Import graph file",
              (Vector2){import_label.x, import_label.y + ui_size(options, 7.0f)},
              ui_size(options, 16.0f), theme->text);

    Rectangle import_row = settings_section_row(options, utilities, 3);
    float load_width = ui_size(options, 94.0f);
    Rectangle path_box = {import_row.x, import_row.y,
                          import_row.width - load_width - ui_size(options, 10.0f),
                          import_row.height};
    Rectangle load_button = {path_box.x + path_box.width + ui_size(options, 10.0f),
                             import_row.y, load_width, import_row.height};

    if (GuiTextBox(path_box, options->graph_path, GRAPHE_GRAPH_PATH_MAX,
                   options->graph_path_editing)) {
        options->graph_path_editing = !options->graph_path_editing;
        result.consumed_click = true;
    }
    if (options->graph_file_dirty && !options->graph_path_editing) {
        draw_text(resources, "*",
                  (Vector2){path_box.x + path_box.width - ui_size(options, 20.0f),
                            path_box.y + ui_size(options, 5.0f)},
                  ui_size(options, 22.0f), theme->active);
    }
    if (rounded_button(resources, options, theme, load_button, "Load", false)) {
        result.graph_load_requested = true;
        result.consumed_click = true;
    }

    if (options->status_message[0] != '\0') {
        draw_text(resources, options->status_message,
                  (Vector2){import_row.x, import_row.y + import_row.height +
                                              ui_size(options, 9.0f)},
                  ui_size(options, 14.0f), theme->muted_text);
    }

    draw_settings_section(resources, options, theme, graph_options, "Graph Options");
    if (rounded_choice_button(resources, options, theme,
                              settings_section_row(options, graph_options, 0),
                              options->alphabetical_order
                                  ? "Traversal order: alphabetical"
                                  : "Traversal order: insertion",
                              options->alphabetical_order)) {
        options->alphabetical_order = !options->alphabetical_order;
        result.trace_changed = true;
        result.consumed_click = true;
    }
    if (rounded_choice_button(
            resources, options, theme,
            settings_section_row(options, graph_options, 1),
            options->directed_graph ? "Graph: directed" : "Graph: undirected",
            options->directed_graph)) {
        options->directed_graph = !options->directed_graph;
        result.trace_changed = true;
        result.consumed_click = true;
    }

    return result;
}

/*
 * Rebuilds the word-pattern background as an opaque texture. Baking the theme
 * background into the texture avoids double-applying alpha when the cache is
 * drawn back to the frame.
 */
static void rebuild_graph_word_background(RenderResources *resources,
                                          const RenderOptions *options,
                                          const Theme *theme, int width,
                                          int height) {
    float graph_width = (float)width;
    float graph_height = (float)height;
    const char *words[] = {"graphe", "graph", "dfs", "tree", "edge", "node"};
    const int word_count = 6;
    const float angle = -18.0f;
    float radians = angle * DEG2RAD;
    float cos_a = cosf(radians);
    float sin_a = sinf(radians);
    float step_x = ui_size(options, 82.0f);
    float step_y = ui_size(options, 27.0f);
    int cols = (int)((graph_width + graph_height) / step_x) + 5;
    int rows = (int)((graph_width + graph_height) / step_y) + 5;
    Color colors[] = {
        with_alpha(lighten_color(theme->background, 0.20f),
                   options->dark_mode ? 56 : 50),
        with_alpha(lighten_color(theme->panel, 0.14f), options->dark_mode ? 52 : 46),
        with_alpha(theme->edge_unclassified, options->dark_mode ? 44 : 38),
    };

    unload_graph_background(resources);
    resources->graph_background = LoadRenderTexture(width, height);
    if (resources->graph_background.texture.id == 0) return;

    resources->graph_background_loaded = true;
    resources->graph_background_width = width;
    resources->graph_background_height = height;
    resources->graph_background_dark_mode = options->dark_mode;
    resources->graph_background_ui_scale = ui_scale_value(options);

    BeginTextureMode(resources->graph_background);
    ClearBackground(theme->background);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            float local_x = -graph_height * 0.55f + (float)col * step_x +
                            (float)(row % 2) * step_x * 0.5f;
            float local_y = -graph_width * 0.18f + (float)row * step_y;
            float center_x = graph_width * 0.5f;
            float center_y = graph_height * 0.5f;
            float dx = local_x - center_x;
            float dy = local_y - center_y;
            float x = center_x + dx * cos_a - dy * sin_a;
            float y = center_y + dx * sin_a + dy * cos_a;
            int word_index = (row + col * 2) % word_count;
            int color_index = (row * 3 + col * 5) % 3;
            float font_size = ui_size(options, 15.0f + (float)((row + col) % 5));

            DrawTextPro(resources->font, words[word_index], (Vector2){x, y},
                        (Vector2){0.0f, 0.0f}, angle, font_size, font_size * 0.20f,
                        colors[color_index]);
        }
    }

    EndTextureMode();
}

/*
 * Keeps the cached background in sync with the area that will draw it. The cache
 * is shared by graph and settings views, so callers pass the target size.
 */
static void ensure_graph_word_background(RenderResources *resources,
                                         const RenderOptions *options,
                                         const Theme *theme, int width, int height) {
    if (width <= 0 || height <= 0) {
        unload_graph_background(resources);
        return;
    }

    bool background_stale =
        !resources->graph_background_loaded ||
        resources->graph_background_width != width ||
        resources->graph_background_height != height ||
        resources->graph_background_dark_mode != options->dark_mode ||
        resources->graph_background_ui_scale != ui_scale_value(options);

    if (background_stale)
        rebuild_graph_word_background(resources, options, theme, width, height);
}

static void draw_graph_word_background(const RenderResources *resources) {
    if (!resources->graph_background_loaded) return;

    Rectangle source = {
        0.0f,
        0.0f,
        (float)resources->graph_background_width,
        -(float)resources->graph_background_height,
    };
    DrawTextureRec(resources->graph_background.texture, source,
                   (Vector2){0.0f, 0.0f}, WHITE);
}

#define GRAPHE_EVENT_BANNER_LINE_COUNT 2
#define GRAPHE_EVENT_BANNER_LINE_MAX 192

static void copy_text_range(char *out, size_t out_size, const char *text,
                            size_t length) {
    if (out_size == 0) return;
    if (length >= out_size) length = out_size - 1;

    memcpy(out, text, length);
    out[length] = '\0';
}

/* Splits at the word boundary that produces the most balanced two lines. */
static int split_event_banner_text(
    const RenderResources *resources, const char *text, float text_size,
    char lines[GRAPHE_EVENT_BANNER_LINE_COUNT][GRAPHE_EVENT_BANNER_LINE_MAX]) {
    size_t length = strlen(text);
    size_t best_split = 0;
    float best_width = -1.0f;
    char left[GRAPHE_EVENT_BANNER_LINE_MAX];
    char right[GRAPHE_EVENT_BANNER_LINE_MAX];

    for (size_t i = 1; i < length; i++) {
        if (text[i] != ' ') continue;

        size_t right_start = i + 1;
        while (right_start < length && text[right_start] == ' ') right_start++;
        if (right_start >= length) continue;

        copy_text_range(left, sizeof(left), text, i);
        snprintf(right, sizeof(right), "%s", text + right_start);
        float left_width = measure_text(resources, left, text_size).x;
        float right_width = measure_text(resources, right, text_size).x;
        float widest = left_width > right_width ? left_width : right_width;

        if (best_width < 0.0f || widest < best_width) {
            best_width = widest;
            best_split = i;
        }
    }

    if (best_split == 0) {
        snprintf(lines[0], GRAPHE_EVENT_BANNER_LINE_MAX, "%s", text);
        lines[1][0] = '\0';
        return 1;
    }

    size_t right_start = best_split + 1;
    while (right_start < length && text[right_start] == ' ') right_start++;
    copy_text_range(lines[0], GRAPHE_EVENT_BANNER_LINE_MAX, text, best_split);
    snprintf(lines[1], GRAPHE_EVENT_BANNER_LINE_MAX, "%s", text + right_start);
    return 2;
}

static float event_banner_lines_width(
    const RenderResources *resources,
    char lines[GRAPHE_EVENT_BANNER_LINE_COUNT][GRAPHE_EVENT_BANNER_LINE_MAX],
    int line_count, float text_size) {
    float width = 0.0f;

    for (int i = 0; i < line_count; i++) {
        float line_width = measure_text(resources, lines[i], text_size).x;
        if (line_width > width) width = line_width;
    }

    return width;
}

static void fit_event_banner_line(const RenderResources *resources, char *line,
                                  size_t line_size, float text_size,
                                  float max_width) {
    if (measure_text(resources, line, text_size).x <= max_width) return;

    char original[GRAPHE_EVENT_BANNER_LINE_MAX];
    size_t length = 0;
    size_t source_limit = line_size > 0 ? line_size - 1 : 0;
    if (source_limit >= sizeof(original)) source_limit = sizeof(original) - 1;
    while (length < source_limit && line[length] != '\0') length++;
    copy_text_range(original, sizeof(original), line, length);

    while (length > 0) {
        length--;
        while (length > 0 && ((unsigned char)original[length] & 0xc0u) == 0x80u)
            length--;

        snprintf(line, line_size, "%.*s...", (int)length, original);
        if (measure_text(resources, line, text_size).x <= max_width) return;
    }

    snprintf(line, line_size, "...");
}

static void draw_event_banner(const Graph *graph, const Trace *trace,
                              size_t active_index, const RenderResources *resources,
                              const RenderOptions *options, const Theme *theme) {
    char event_text[192];
    describe_event(graph, trace, active_index, options, event_text,
                   sizeof(event_text));

    float graph_width = render_graph_area_width(options);
    float graph_height = render_graph_area_height();
    float side_margin = ui_size(options, 40.0f);
    if (side_margin > graph_width * 0.12f) side_margin = graph_width * 0.12f;
    float max_width = graph_width - side_margin * 2.0f;
    float padding_x = ui_size(options, 22.0f);
    float padding_y = ui_size(options, 13.0f);
    if (padding_x * 2.0f > max_width * 0.25f) padding_x = max_width * 0.125f;
    float max_line_width = max_width - padding_x * 2.0f;
    float text_size = ui_size(options, 22.0f);
    float min_text_size = ui_size(options, 16.0f);
    float text_size_step = ui_size(options, 1.0f);
    char lines[GRAPHE_EVENT_BANNER_LINE_COUNT][GRAPHE_EVENT_BANNER_LINE_MAX] = {{0}};
    int line_count;
    if (measure_text(resources, event_text, text_size).x <= max_line_width) {
        snprintf(lines[0], sizeof(lines[0]), "%s", event_text);
        line_count = 1;
    } else {
        line_count =
            split_event_banner_text(resources, event_text, text_size, lines);
    }
    float lines_width =
        event_banner_lines_width(resources, lines, line_count, text_size);

    while (lines_width > max_line_width &&
           text_size - text_size_step >= min_text_size) {
        text_size -= text_size_step;
        lines_width =
            event_banner_lines_width(resources, lines, line_count, text_size);
    }

    for (int i = 0; i < line_count; i++)
        fit_event_banner_line(resources, lines[i], sizeof(lines[i]), text_size,
                              max_line_width);

    lines_width = event_banner_lines_width(resources, lines, line_count, text_size);
    float width = lines_width + padding_x * 2.0f;

    if (width > max_width) width = max_width;

    float line_gap = line_count > 1 ? ui_size(options, 5.0f) : 0.0f;
    float height = text_size * (float)line_count +
                   line_gap * (float)(line_count - 1) + padding_y * 2.0f;
    Rectangle box = {
        (graph_width - width) * 0.5f,
        graph_height - height - ui_size(options, 28.0f),
        width,
        height,
    };
    Color fill = with_alpha(theme->panel, 255);

    DrawRectangleRounded(box, 0.18f, 8, fill);
    DrawRectangleRoundedLines(box, 0.18f, 8, theme->panel_border);
    for (int i = 0; i < line_count; i++) {
        Vector2 line_size = measure_text(resources, lines[i], text_size);
        Vector2 position = {
            box.x + (box.width - line_size.x) * 0.5f,
            box.y + padding_y - ui_size(options, 1.0f) +
                (text_size + line_gap) * (float)i,
        };
        draw_text(resources, lines[i], position, text_size, theme->text);
    }
}

static RenderUiResult draw_sidebar(const Graph *graph, const Trace *trace,
                                   size_t applied_event_count,
                                   const RenderResources *resources,
                                   RenderOptions *options, const Theme *theme) {
    RenderUiResult result = {0};
    float x = sidebar_left(options) + ui_size(options, 28.0f);
    float y = ui_size(options, 30.0f);
    float line = ui_size(options, 28.0f);
    float sidebar_width = render_sidebar_width(options);
    char progress[64];
    char algorithm_text[64];
    char graph_text[64];
    char layout_text[64];
    char scale_text[64];

    snprintf(progress, sizeof(progress), "Step %d / %d", (int)applied_event_count,
             (int)trace->count);
    snprintf(algorithm_text, sizeof(algorithm_text), "Algorithm: %s",
             algorithm_mode_name(options->algorithm_mode));
    snprintf(graph_text, sizeof(graph_text), "Graph: %s",
             graph->directed ? "directed" : "undirected");
    snprintf(layout_text, sizeof(layout_text), "Layout: %s",
             layout_mode_name(options->layout_mode));
    snprintf(scale_text, sizeof(scale_text), "UI scale: %.0f%%",
             ui_scale_value(options) * 100.0f);

    DrawRectangle((int)sidebar_left(options), 0, (int)sidebar_width,
                  GetScreenHeight(), theme->panel);
    DrawLine((int)sidebar_left(options), 0, (int)sidebar_left(options),
             GetScreenHeight(), theme->panel_border);

    draw_text(resources, "Graphe", (Vector2){x, y}, ui_size(options, 34.0f),
              theme->text);

    if (rounded_button(resources, options, theme, settings_button_rect(options),
                       "Settings", false)) {
        options->settings_open = !options->settings_open;
        result.consumed_click = true;
    }

    draw_text(resources, algorithm_text, (Vector2){x, y + line * 1.8f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, progress, (Vector2){x, y + line * 2.65f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, graph_text, (Vector2){x, y + line * 3.55f},
              ui_size(options, 19.0f), theme->muted_text);
    if (options->algorithm_mode == ALGORITHM_TREE) {
        char order_text[64];
        snprintf(order_text, sizeof(order_text), "Tree order: %s",
                 tree_traversal_order_name(options->tree_order));
        draw_text(resources, order_text, (Vector2){x, y + line * 4.45f},
                  ui_size(options, 19.0f), theme->muted_text);
    } else if (options->algorithm_mode == ALGORITHM_BFS) {
        char level_text[80];
        snprintf(level_text, sizeof(level_text), "Reached: %d/%d, max level: %d",
                 node_count_with_level(graph), (int)graph->node_count,
                 max_bfs_level(graph));
        draw_text(resources, level_text, (Vector2){x, y + line * 4.45f},
                  ui_size(options, 19.0f), theme->muted_text);
    } else if (options->algorithm_mode == ALGORITHM_DIJKSTRA) {
        char dijkstra_text[96];
        int source = dijkstra_source_node(trace);
        snprintf(dijkstra_text, sizeof(dijkstra_text), "Source: %s, settled: %d/%d",
                 source >= 0 ? graph->nodes[source].label : "-",
                 settled_node_count(graph), (int)graph->node_count);
        draw_text(resources, dijkstra_text, (Vector2){x, y + line * 4.45f},
                  ui_size(options, 19.0f), theme->muted_text);
    } else {
        draw_text(resources, layout_text, (Vector2){x, y + line * 4.45f},
                  ui_size(options, 19.0f), theme->muted_text);
    }
    draw_text(resources, scale_text, (Vector2){x, y + line * 5.35f},
              ui_size(options, 19.0f), theme->muted_text);

    draw_text(resources, "Controls", (Vector2){x, y + line * 6.85f},
              ui_size(options, 22.0f), theme->text);
    draw_text(resources, "Space: play / pause", (Vector2){x, y + line * 7.85f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, "Right / Left: step", (Vector2){x, y + line * 8.7f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, "0 / Enter: jump", (Vector2){x, y + line * 9.55f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, "R: reset layout", (Vector2){x, y + line * 10.4f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, "M: cycle algorithm", (Vector2){x, y + line * 11.25f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, "Ctrl +/-/0: UI scale", (Vector2){x, y + line * 12.1f},
              ui_size(options, 19.0f), theme->muted_text);
    draw_text(resources, "Tab: settings", (Vector2){x, y + line * 12.95f},
              ui_size(options, 19.0f), theme->muted_text);

    if (options->algorithm_mode == ALGORITHM_TREE) {
        float inner_width = sidebar_width - ui_size(options, 56.0f);
        float gap = ui_size(options, 8.0f);
        float button_width = (inner_width - gap * 2.0f) / 3.0f;
        float button_y = y + line * 16.0f;

        draw_text(resources, "Traversal Order", (Vector2){x, y + line * 15.0f},
                  ui_size(options, 22.0f), theme->text);

        Rectangle preorder = {x, button_y, button_width, ui_size(options, 34.0f)};
        Rectangle inorder = {x + button_width + gap, button_y, button_width,
                             ui_size(options, 34.0f)};
        Rectangle postorder = {x + (button_width + gap) * 2.0f, button_y,
                               button_width, ui_size(options, 34.0f)};

        if (rounded_button(resources, options, theme, preorder, "Pre",
                           options->tree_order == TREE_ORDER_PREORDER) &&
            options->tree_order != TREE_ORDER_PREORDER) {
            options->tree_order = TREE_ORDER_PREORDER;
            result.trace_changed = true;
            result.consumed_click = true;
        }
        if (rounded_button(resources, options, theme, inorder, "In",
                           options->tree_order == TREE_ORDER_INORDER) &&
            options->tree_order != TREE_ORDER_INORDER) {
            options->tree_order = TREE_ORDER_INORDER;
            result.trace_changed = true;
            result.consumed_click = true;
        }
        if (rounded_button(resources, options, theme, postorder, "Post",
                           options->tree_order == TREE_ORDER_POSTORDER) &&
            options->tree_order != TREE_ORDER_POSTORDER) {
            options->tree_order = TREE_ORDER_POSTORDER;
            result.trace_changed = true;
            result.consumed_click = true;
        }

        return result;
    }

    if (options->algorithm_mode == ALGORITHM_DIJKSTRA) {
        float heading_y = y + line * 15.0f;
        float item_x = x + ui_size(options, 22.0f);
        float tentative_y = y + line * 16.0f;
        float settled_y = y + line * 16.8f;
        float predecessor_y = y + line * 17.6f;
        float marker_x = x + ui_size(options, 7.0f);
        float marker_radius = ui_size(options, 5.0f);

        draw_text(resources, "Dijkstra State", (Vector2){x, heading_y},
                  ui_size(options, 22.0f), theme->text);
        DrawCircleV((Vector2){marker_x, tentative_y + ui_size(options, 9.0f)},
                    marker_radius, theme->node_gray);
        DrawCircleV((Vector2){marker_x, settled_y + ui_size(options, 9.0f)},
                    marker_radius, theme->node_black);
        DrawLineEx((Vector2){marker_x - marker_radius,
                             predecessor_y + ui_size(options, 9.0f)},
                   (Vector2){marker_x + marker_radius,
                             predecessor_y + ui_size(options, 9.0f)},
                   ui_size(options, 3.0f), theme->edge_tree);
        draw_text(resources, "Tentative distance", (Vector2){item_x, tentative_y},
                  ui_size(options, 19.0f), theme->muted_text);
        draw_text(resources, "Settled distance", (Vector2){item_x, settled_y},
                  ui_size(options, 19.0f), theme->muted_text);
        draw_text(resources, "Current predecessor", (Vector2){item_x, predecessor_y},
                  ui_size(options, 19.0f), theme->muted_text);
        return result;
    }

    if (options->algorithm_mode != ALGORITHM_DFS) return result;

    draw_text(resources, "Edge Types", (Vector2){x, y + line * 15.0f},
              ui_size(options, 22.0f), theme->text);

    const EdgeType edge_types[] = {EDGE_TREE, EDGE_BACK, EDGE_FORWARD, EDGE_CROSS};
    const char *labels[] = {"Tree", "Back", "Forward", "Cross"};
    float item_x = x + ui_size(options, 22.0f);
    float marker_x = x + ui_size(options, 7.0f);
    float marker_half_width = ui_size(options, 6.0f);
    for (int i = 0; i < 4; i++) {
        float item_y = y + 1.0f + line * (16.0f + (float)i * 0.8f);
        Color color = edge_color(theme, edge_types[i]);
        float marker_y = item_y + ui_size(options, 9.0f);
        DrawLineEx((Vector2){marker_x - marker_half_width, marker_y},
                   (Vector2){marker_x + marker_half_width, marker_y},
                   ui_size(options, 3.0f), color);
        draw_text(resources, labels[i], (Vector2){item_x, item_y},
                  ui_size(options, 19.0f), theme->muted_text);
    }

    return result;
}

/*
 * Top-level draw pass for the application. Edges and arrowheads are drawn before
 * nodes so nodes stay on top; sidebar/settings UI is drawn after the graph scene.
 */
RenderUiResult render_graph(const Graph *graph, const Trace *trace,
                            size_t applied_event_count, RenderResources *resources,
                            RenderOptions *options, const Camera2D *camera) {
    RenderUiResult result = {0};
    Theme theme = theme_for_options(options);
    size_t active_index =
        applied_event_count == 0 ? trace->count : applied_event_count - 1;

    apply_gui_style(resources, options, &theme);

    if (options->settings_open) {
        merge_ui_result(&result, draw_settings(resources, options, &theme));
        DrawRectangle(0, 0, GetScreenWidth(), 1, theme.panel_border);
        return result;
    }

    ensure_graph_word_background(resources, options, &theme,
                                 (int)ceilf(render_graph_area_width(options)),
                                 (int)ceilf(render_graph_area_height()));
    draw_graph_word_background(resources);

    BeginScissorMode(0, 0, (int)render_graph_area_width(options),
                     (int)render_graph_area_height());
    BeginMode2D(*camera);

    for (size_t i = 0; i < graph->edge_count; i++) {
        if (!graph_edge_is_visible(graph, (int)i)) continue;
        int active_edge = is_active_edge(graph, trace, active_index, i);
        int active_examine = is_active_examine_edge(graph, trace, active_index, i);
        EdgeGeometry geometry =
            make_edge_geometry(resources, graph, i, active_edge, options);

        draw_edge_body(graph, i, geometry, active_examine, &theme, options);
        if (graph->directed)
            draw_edge_arrow(graph, i, geometry, active_examine, &theme, options);
    }

    if (options->algorithm_mode == ALGORITHM_DIJKSTRA) {
        for (size_t i = 0; i < graph->edge_count; i++) {
            if (!graph_edge_is_visible(graph, (int)i)) continue;
            int active_edge = is_active_edge(graph, trace, active_index, i);
            EdgeGeometry geometry =
                make_edge_geometry(resources, graph, i, active_edge, options);
            draw_edge_weight(resources, &graph->edges[i], geometry, &theme);
        }
    }

    for (size_t i = 0; i < graph->node_count; i++)
        draw_node(resources, &graph->nodes[i],
                  is_active_node(trace, active_index, i), &theme, options);

    EndMode2D();
    EndScissorMode();

    draw_event_banner(graph, trace, active_index, resources, options, &theme);

    merge_ui_result(&result, draw_sidebar(graph, trace, applied_event_count,
                                          resources, options, &theme));

    DrawRectangle(0, 0, GetScreenWidth(), 1, theme.panel_border);

    return result;
}
