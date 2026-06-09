#include "Menu/line_drawing_catalog_preview.h"

#include "Tools/canonical_scene_export_primitives.h"
#include "Tools/scene_import.h"
#include "Layout/layout_json.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    LINE_DRAWING_CATALOG_PREVIEW_MAX_RAW_POINTS =
        LINE_DRAWING_CATALOG_PREVIEW_MAX_SEGMENTS * 2
};

typedef struct LineDrawingCatalogPreviewRawSegment {
    Vec2 a;
    Vec2 b;
} LineDrawingCatalogPreviewRawSegment;

typedef struct LineDrawingCatalogPreviewRawGeometry {
    int segment_count;
    LineDrawingCatalogPreviewRawSegment segments[LINE_DRAWING_CATALOG_PREVIEW_MAX_SEGMENTS];
} LineDrawingCatalogPreviewRawGeometry;

static const int k_rect_prism_edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

static bool line_drawing_catalog_preview_load_layout(Layout* layout,
                                                     LineDrawingCatalogPreviewSourceKind kind,
                                                     const char* path,
                                                     char* diagnostics,
                                                     size_t diagnostics_size) {
    if (!layout || !path || !path[0]) return false;
    if (kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT) {
        if (Layout_LoadFromFile(layout, path)) {
            if (diagnostics && diagnostics_size > 0u) diagnostics[0] = '\0';
            return true;
        }
        if (diagnostics && diagnostics_size > 0u) {
            snprintf(diagnostics, diagnostics_size, "layout file could not be loaded");
        }
        return false;
    }
    return LineDrawingSceneImport_LoadLayoutFromAuthoringFile(layout,
                                                              path,
                                                              diagnostics,
                                                              diagnostics_size);
}

static void line_drawing_catalog_preview_accumulate_counts(const Layout* layout,
                                                           LineDrawingCatalogPreviewData* preview) {
    size_t i = 0u;
    if (!layout || !preview) return;
    for (i = 0u; i < layout->anchorCount; ++i) {
        if (!layout->anchors[i].isDeleted) {
            preview->anchor_count += 1;
        }
    }
    for (i = 0u; i < layout->wallCount; ++i) {
        if (!layout->walls[i].isDeleted) {
            preview->wall_count += 1;
        }
    }
    for (i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (object->isDeleted) continue;
        preview->object_count += 1;
        if (object->kind == OBJECT3D_KIND_PLANE) {
            preview->plane_count += 1;
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            preview->rect_prism_count += 1;
        }
    }
}

static void line_drawing_catalog_preview_project_bounds(const Layout* layout,
                                                        Vec3* out_center,
                                                        float* out_half_extent,
                                                        LineDrawingCatalogPreviewData* preview) {
    SceneBounds3D bounds = {0};
    Vec3 center = {0};
    Vec3 span = {0};
    float max_dim = 1.0f;
    if (!layout || !out_center || !out_half_extent || !preview) return;

    if (!LineDrawingCanonicalScene_ComputeFramingBounds(layout, &bounds)) {
        bounds.enabled = true;
        bounds.min = (Vec3){-1.0f, -1.0f, -1.0f};
        bounds.max = (Vec3){1.0f, 1.0f, 1.0f};
    }

    span = Vec3_Sub(bounds.max, bounds.min);
    center = Vec3_Scale(Vec3_Add(bounds.min, bounds.max), 0.5f);
    max_dim = fmaxf(fmaxf(span.x, span.y), span.z);
    if (max_dim < 1e-3f) max_dim = 1.0f;

    preview->extent_x = fabsf(span.x);
    preview->extent_y = fabsf(span.y);
    preview->extent_z = fabsf(span.z);
    *out_center = center;
    *out_half_extent = max_dim * 0.5f;
}

static Vec2 line_drawing_catalog_preview_project_point(Vec3 point, const FreeViewCamera* camera) {
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
    return Vec3_ProjectToView(point, plane, camera);
}

static bool line_drawing_catalog_preview_add_raw_segment(LineDrawingCatalogPreviewRawGeometry* raw,
                                                         Vec2 a,
                                                         Vec2 b) {
    if (!raw) return false;
    if (raw->segment_count < 0 ||
        raw->segment_count >= LINE_DRAWING_CATALOG_PREVIEW_MAX_SEGMENTS) {
        return false;
    }
    raw->segments[raw->segment_count].a = a;
    raw->segments[raw->segment_count].b = b;
    raw->segment_count += 1;
    return true;
}

static void line_drawing_catalog_preview_build_raw_geometry(const Layout* layout,
                                                            const FreeViewCamera* camera,
                                                            LineDrawingCatalogPreviewRawGeometry* raw) {
    size_t i = 0u;
    if (!layout || !camera || !raw) return;

    for (i = 0u; i < layout->wallCount; ++i) {
        const Wall* wall = &layout->walls[i];
        Vec2 a = {0};
        Vec2 b = {0};
        if (wall->isDeleted) continue;
        if (wall->anchorA < 0 || wall->anchorB < 0) continue;
        if ((size_t)wall->anchorA >= layout->anchorCount || (size_t)wall->anchorB >= layout->anchorCount) continue;
        if (layout->anchors[wall->anchorA].isDeleted || layout->anchors[wall->anchorB].isDeleted) continue;
        a = line_drawing_catalog_preview_project_point(layout->anchors[wall->anchorA].pos, camera);
        b = line_drawing_catalog_preview_project_point(layout->anchors[wall->anchorB].pos, camera);
        (void)line_drawing_catalog_preview_add_raw_segment(raw, a, b);
    }

    for (i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (object->isDeleted) continue;
        if (object->kind == OBJECT3D_KIND_PLANE) {
            Vec3 corners3[4] = {0};
            int edge = 0;
            if (!Layout_Object3D_ComputePlaneCorners(object, corners3)) continue;
            for (edge = 0; edge < 4; ++edge) {
                const int next = (edge + 1) % 4;
                (void)line_drawing_catalog_preview_add_raw_segment(
                    raw,
                    line_drawing_catalog_preview_project_point(corners3[edge], camera),
                    line_drawing_catalog_preview_project_point(corners3[next], camera));
            }
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            Vec3 corners3[8] = {0};
            int edge = 0;
            if (!Layout_Object3D_ComputeRectPrismCorners(object, corners3)) continue;
            for (edge = 0; edge < 12; ++edge) {
                const Vec3 a3 = corners3[k_rect_prism_edges[edge][0]];
                const Vec3 b3 = corners3[k_rect_prism_edges[edge][1]];
                (void)line_drawing_catalog_preview_add_raw_segment(
                    raw,
                    line_drawing_catalog_preview_project_point(a3, camera),
                    line_drawing_catalog_preview_project_point(b3, camera));
            }
        }
        if (raw->segment_count >= LINE_DRAWING_CATALOG_PREVIEW_MAX_SEGMENTS) {
            return;
        }
    }
}

static void line_drawing_catalog_preview_normalize(LineDrawingCatalogPreviewData* preview,
                                                   const LineDrawingCatalogPreviewRawGeometry* raw) {
    int i = 0;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float span_x = 1.0f;
    float span_y = 1.0f;
    const float pad = 0.08f;

    if (!preview || !raw || raw->segment_count <= 0) return;

    min_x = max_x = raw->segments[0].a.x;
    min_y = max_y = raw->segments[0].a.y;
    for (i = 0; i < raw->segment_count; ++i) {
        const Vec2 points[2] = {raw->segments[i].a, raw->segments[i].b};
        int p = 0;
        for (p = 0; p < 2; ++p) {
            if (points[p].x < min_x) min_x = points[p].x;
            if (points[p].x > max_x) max_x = points[p].x;
            if (points[p].y < min_y) min_y = points[p].y;
            if (points[p].y > max_y) max_y = points[p].y;
        }
    }

    span_x = max_x - min_x;
    span_y = max_y - min_y;
    if (span_x < 1e-4f) span_x = 1.0f;
    if (span_y < 1e-4f) span_y = 1.0f;

    preview->segment_count = raw->segment_count;
    for (i = 0; i < raw->segment_count; ++i) {
        const LineDrawingCatalogPreviewRawSegment* src = &raw->segments[i];
        LineDrawingCatalogPreviewSegment* dst = &preview->segments[i];
        dst->x0 = pad + ((src->a.x - min_x) / span_x) * (1.0f - (pad * 2.0f));
        dst->y0 = pad + ((src->a.y - min_y) / span_y) * (1.0f - (pad * 2.0f));
        dst->x1 = pad + ((src->b.x - min_x) / span_x) * (1.0f - (pad * 2.0f));
        dst->y1 = pad + ((src->b.y - min_y) / span_y) * (1.0f - (pad * 2.0f));
    }
    preview->has_preview = preview->segment_count > 0;
}

bool LineDrawingCatalogPreview_Load(LineDrawingCatalogPreviewData* out_preview,
                                    LineDrawingCatalogPreviewSourceKind kind,
                                    const char* path) {
    Layout layout;
    Vec3 center = {0};
    float half_extent = 1.0f;
    FreeViewCamera camera = {0};
    LineDrawingCatalogPreviewRawGeometry raw = {0};
    char diagnostics[128] = {0};

    if (!out_preview) return false;
    memset(out_preview, 0, sizeof(*out_preview));
    out_preview->loaded = true;
    Layout_Init(&layout, 1.0f);

    if (!line_drawing_catalog_preview_load_layout(&layout,
                                                  kind,
                                                  path,
                                                  diagnostics,
                                                  sizeof(diagnostics))) {
        out_preview->load_failed = true;
        snprintf(out_preview->diagnostics, sizeof(out_preview->diagnostics), "%s", diagnostics);
        Layout_Free(&layout);
        return false;
    }

    line_drawing_catalog_preview_accumulate_counts(&layout, out_preview);
    line_drawing_catalog_preview_project_bounds(&layout, &center, &half_extent, out_preview);

    camera.enabled = true;
    camera.yawDeg = 36.0f;
    camera.pitchDeg = 24.0f;
    camera.target = center;

    line_drawing_catalog_preview_build_raw_geometry(&layout, &camera, &raw);
    line_drawing_catalog_preview_normalize(out_preview, &raw);
    if (!out_preview->has_preview) {
        snprintf(out_preview->diagnostics,
                 sizeof(out_preview->diagnostics),
                 "No drawable wireframe available");
    }

    Layout_Free(&layout);
    return !out_preview->load_failed;
}

void LineDrawingCatalogPreviewCache_Init(LineDrawingCatalogPreviewCache* cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

const LineDrawingCatalogPreviewData* LineDrawingCatalogPreviewCache_Get(
    LineDrawingCatalogPreviewCache* cache,
    LineDrawingCatalogPreviewSourceKind kind,
    const char* path) {
    int i = 0;
    int slot = 0;
    if (!cache || !path || !path[0]) return NULL;

    for (i = 0; i < LINE_DRAWING_CATALOG_PREVIEW_CACHE_CAPACITY; ++i) {
        if (!cache->entries[i].occupied) continue;
        if (cache->entries[i].kind == kind && strcmp(cache->entries[i].path, path) == 0) {
            return &cache->entries[i].data;
        }
    }

    slot = cache->next_replace_index;
    if (slot < 0 || slot >= LINE_DRAWING_CATALOG_PREVIEW_CACHE_CAPACITY) {
        slot = 0;
    }
    cache->entries[slot].occupied = true;
    cache->entries[slot].kind = kind;
    snprintf(cache->entries[slot].path, sizeof(cache->entries[slot].path), "%s", path);
    (void)LineDrawingCatalogPreview_Load(&cache->entries[slot].data, kind, path);
    cache->next_replace_index =
        (slot + 1) % LINE_DRAWING_CATALOG_PREVIEW_CACHE_CAPACITY;
    return &cache->entries[slot].data;
}
