#include "Layout/scene/layout_mesh_runtime_preview.h"

#include "Layout/scene/layout_mesh_preview_sidecar.h"
#include "core_io.h"

#include "cjson/cJSON.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LD_MESH_PREVIEW_MAX_VERTICES 65536u
#define LD_MESH_PREVIEW_RUNTIME_PARSE_LIMIT_BYTES (8u * 1024u * 1024u)

typedef struct {
    char runtimePath[512];
    bool populated;
    bool valid;
    LayoutMeshRuntimePreviewStats stats;
    Vec3 vertices[LD_MESH_PREVIEW_MAX_VERTICES];
    LayoutMeshPreviewSidecarEdge edges[LD_MESH_PREVIEW_MAX_EDGES];
} LayoutMeshRuntimePreviewCache;

static LayoutMeshRuntimePreviewCache g_mesh_preview_cache;

static void MeshPreview_SetDiagnostics(char* diagnostics,
                                       size_t diagnosticsSize,
                                       const char* message) {
    if (!diagnostics || diagnosticsSize == 0u) return;
    snprintf(diagnostics, diagnosticsSize, "%s", message ? message : "");
}

static void MeshPreview_CopyMetadataToStats(const LayoutMeshPreviewSidecarMetadata* metadata,
                                            LayoutMeshRuntimePreviewStats* stats,
                                            bool metadataOnly) {
    if (!metadata || !stats) return;
    snprintf(stats->previewMode,
             sizeof(stats->previewMode),
             "%s",
             metadata->previewMode[0] ? metadata->previewMode : "unknown");
    snprintf(stats->sampleStrategy,
             sizeof(stats->sampleStrategy),
             "%s",
             metadata->sampleStrategy[0] ? metadata->sampleStrategy : "unknown");
    stats->sourceVertexCount = metadata->sourceVertexCount;
    stats->sourceTriangleCount = metadata->sourceTriangleCount;
    stats->previewVertexCount = metadata->previewVertexCount;
    stats->previewEdgeCount = metadata->previewEdgeCount;
    stats->previewTriangleCount = metadata->previewTriangleCount;
    stats->maxBudget = metadata->maxBudget;
    stats->edgeCount = metadata->edgeCount;
    stats->coverageRatio = metadata->coverageRatio;
    stats->maxSpan = metadata->maxSpan;
    stats->boundingSphereRadius = metadata->boundingSphereRadius;
    stats->localBoundsMin = metadata->localBoundsMin;
    stats->localBoundsMax = metadata->localBoundsMax;
    stats->boundsCenter = metadata->boundsCenter;
    stats->boundsExtent = metadata->boundsExtent;
    stats->hasDrawablePayload = metadata->hasDrawablePayload;
    stats->metadataOnly = metadataOnly;
    stats->loadedVertexCount = metadata->sourceVertexCount;
    stats->vertexTruncated = metadata->sourceVertexCount > LD_MESH_PREVIEW_MAX_VERTICES;
    stats->triangleSampled = metadata->previewTriangleCount < metadata->sourceTriangleCount;
}

static bool MeshPreview_Vec3FromJson(const cJSON* node, Vec3* out) {
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    const cJSON* z = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    out->x = (float)x->valuedouble;
    out->y = (float)y->valuedouble;
    out->z = (float)z->valuedouble;
    return true;
}

static bool MeshPreview_TriangleIndexFromJson(const cJSON* node,
                                              const char* key,
                                              size_t* outIndex) {
    const cJSON* value = NULL;
    if (!cJSON_IsObject(node) || !key || !outIndex) return false;
    value = cJSON_GetObjectItemCaseSensitive(node, key);
    if (!cJSON_IsNumber(value) || value->valuedouble < 0.0) return false;
    *outIndex = (size_t)value->valuedouble;
    return true;
}

static Vec3 MeshPreview_TransformPoint(const Object3D* object, Vec3 local) {
    return Layout_Transform3D_ApplyLocalPoint(object->transform, local);
}

static bool MeshPreview_LoadRuntimePath(const char* runtimePath,
                                        LayoutMeshRuntimePreviewCache* cache,
                                        char* diagnostics,
                                        size_t diagnosticsSize) {
    CoreBuffer buffer = {0};
    CoreResult readResult = {0};
    char* text = NULL;
    cJSON* root = NULL;
    const cJSON* schema = NULL;
    const cJSON* mesh = NULL;
    const cJSON* vertices = NULL;
    const cJSON* triangles = NULL;
    int vertexCount = 0;
    int triangleCount = 0;
    int stride = 1;
    bool ok = false;

    if (!runtimePath || !runtimePath[0] || !cache) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "missing runtime mesh preview path");
        return false;
    }

    memset(cache, 0, sizeof(*cache));
    snprintf(cache->runtimePath, sizeof(cache->runtimePath), "%s", runtimePath);
    cache->populated = true;

    if (Layout_MeshPreviewSidecarReadEdges(runtimePath,
                                           cache->edges,
                                           LD_MESH_PREVIEW_MAX_EDGES,
                                           &cache->stats.edgeCount,
                                           &cache->stats.sourceVertexCount,
                                           &cache->stats.sourceTriangleCount,
                                           &cache->stats.previewVertexCount,
                                           &cache->stats.previewEdgeCount,
                                           &cache->stats.previewTriangleCount,
                                           &cache->stats.maxBudget,
                                           &cache->stats.coverageRatio,
                                           &cache->stats.maxSpan,
                                           &cache->stats.boundingSphereRadius,
                                           diagnostics,
                                           diagnosticsSize)) {
        LayoutMeshPreviewSidecarMetadata metadata = {0};
        if (Layout_MeshPreviewSidecarReadMetadata(runtimePath, &metadata, NULL, 0u)) {
            const size_t copiedEdgeCount = cache->stats.edgeCount;
            MeshPreview_CopyMetadataToStats(&metadata, &cache->stats, false);
            cache->stats.edgeCount = copiedEdgeCount;
        } else {
            snprintf(cache->stats.previewMode,
                     sizeof(cache->stats.previewMode),
                     "feature_edges_v1");
            snprintf(cache->stats.sampleStrategy,
                     sizeof(cache->stats.sampleStrategy),
                     "feature_edge_stride_v1");
            cache->stats.hasDrawablePayload = true;
        }
        cache->stats.loadedVertexCount = cache->stats.sourceVertexCount;
        cache->stats.vertexTruncated =
            cache->stats.sourceVertexCount > LD_MESH_PREVIEW_MAX_VERTICES;
        cache->stats.triangleSampled =
            cache->stats.previewTriangleCount < cache->stats.sourceTriangleCount;
        cache->valid = true;
        return true;
    }

    {
        LayoutMeshPreviewSidecarMetadata metadata = {0};
        if (Layout_MeshPreviewSidecarReadMetadata(runtimePath,
                                                  &metadata,
                                                  diagnostics,
                                                  diagnosticsSize)) {
            MeshPreview_CopyMetadataToStats(&metadata, &cache->stats, true);
            cache->valid = true;
            return true;
        }
    }

    {
        struct stat st;
        if (stat(runtimePath, &st) == 0 &&
            st.st_size > (off_t)LD_MESH_PREVIEW_RUNTIME_PARSE_LIMIT_BYTES) {
            MeshPreview_SetDiagnostics(diagnostics,
                                       diagnosticsSize,
                                       "runtime mesh preview requires preview sidecar for large assets");
            return false;
        }
    }

    readResult = core_io_read_all(runtimePath, &buffer);
    if (readResult.code != CORE_OK || !buffer.data || buffer.size == 0u) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "failed to read runtime mesh preview sidecar");
        return false;
    }

    text = (char*)malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "failed to allocate runtime mesh preview buffer");
        return false;
    }
    memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);

    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "failed to parse runtime mesh preview sidecar");
        cJSON_Delete(root);
        return false;
    }

    schema = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    mesh = cJSON_GetObjectItemCaseSensitive(root, "mesh");
    vertices = cJSON_GetObjectItemCaseSensitive(mesh, "vertices");
    triangles = cJSON_GetObjectItemCaseSensitive(mesh, "triangles");
    if (!cJSON_IsString(schema) ||
        strcmp(schema->valuestring, "mesh_asset_runtime_v1") != 0 ||
        !cJSON_IsArray(vertices) ||
        !cJSON_IsArray(triangles)) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "runtime mesh preview sidecar has no mesh arrays");
        goto cleanup;
    }

    vertexCount = cJSON_GetArraySize(vertices);
    triangleCount = cJSON_GetArraySize(triangles);
    if (vertexCount <= 0 || triangleCount <= 0) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "runtime mesh preview sidecar has empty mesh arrays");
        goto cleanup;
    }

    cache->stats.sourceVertexCount = (size_t)vertexCount;
    cache->stats.sourceTriangleCount = (size_t)triangleCount;
    cache->stats.vertexTruncated = (size_t)vertexCount > LD_MESH_PREVIEW_MAX_VERTICES;
    cache->stats.triangleSampled = (size_t)triangleCount > LD_MESH_PREVIEW_MAX_TRIANGLES;
    cache->stats.maxBudget = LD_MESH_PREVIEW_MAX_EDGES;
    snprintf(cache->stats.previewMode, sizeof(cache->stats.previewMode), "runtime_triangle_stride_v1");
    snprintf(cache->stats.sampleStrategy,
             sizeof(cache->stats.sampleStrategy),
             "legacy_runtime_triangle_stride_v1");
    cache->stats.localBoundsMin = (Vec3){0.0f, 0.0f, 0.0f};
    cache->stats.localBoundsMax = (Vec3){0.0f, 0.0f, 0.0f};

    for (int i = 0; i < vertexCount && cache->stats.loadedVertexCount < LD_MESH_PREVIEW_MAX_VERTICES; ++i) {
        Vec3 vertex = {0};
        if (!MeshPreview_Vec3FromJson(cJSON_GetArrayItem(vertices, i), &vertex)) continue;
        cache->vertices[cache->stats.loadedVertexCount++] = vertex;
    }
    if (cache->stats.loadedVertexCount == 0u) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "runtime mesh preview loaded no vertices");
        goto cleanup;
    }

    if ((size_t)triangleCount > LD_MESH_PREVIEW_MAX_TRIANGLES) {
        stride = (int)ceil((double)triangleCount / (double)LD_MESH_PREVIEW_MAX_TRIANGLES);
        if (stride < 1) stride = 1;
    }

    for (int i = 0; i < triangleCount && cache->stats.edgeCount + 3u <= LD_MESH_PREVIEW_MAX_EDGES; i += stride) {
        size_t a = 0u;
        size_t b = 0u;
        size_t c = 0u;
        const cJSON* tri = cJSON_GetArrayItem(triangles, i);
        if (!MeshPreview_TriangleIndexFromJson(tri, "a", &a) ||
            !MeshPreview_TriangleIndexFromJson(tri, "b", &b) ||
            !MeshPreview_TriangleIndexFromJson(tri, "c", &c)) {
            continue;
        }
        if (a >= cache->stats.loadedVertexCount ||
            b >= cache->stats.loadedVertexCount ||
            c >= cache->stats.loadedVertexCount) {
            continue;
        }
        cache->edges[cache->stats.edgeCount++] = (LayoutMeshPreviewSidecarEdge){ cache->vertices[a], cache->vertices[b] };
        cache->edges[cache->stats.edgeCount++] = (LayoutMeshPreviewSidecarEdge){ cache->vertices[b], cache->vertices[c] };
        cache->edges[cache->stats.edgeCount++] = (LayoutMeshPreviewSidecarEdge){ cache->vertices[c], cache->vertices[a] };
        cache->stats.previewTriangleCount++;
    }

    if (cache->stats.edgeCount == 0u) {
        MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, "runtime mesh preview loaded no drawable edges");
        goto cleanup;
    }

    cache->valid = true;
    cache->stats.previewEdgeCount = cache->stats.edgeCount;
    cache->stats.previewVertexCount = cache->stats.edgeCount * 2u;
    cache->stats.hasDrawablePayload = true;
    cache->stats.metadataOnly = false;
    if (cache->stats.sourceTriangleCount > 0u) {
        cache->stats.coverageRatio =
            (double)cache->stats.previewTriangleCount /
            (double)cache->stats.sourceTriangleCount;
        if (cache->stats.coverageRatio > 1.0) cache->stats.coverageRatio = 1.0;
    }
    ok = true;
    MeshPreview_SetDiagnostics(diagnostics, diagnosticsSize, NULL);

cleanup:
    cJSON_Delete(root);
    return ok;
}

static const LayoutMeshRuntimePreviewCache* MeshPreview_CacheForPath(const char* runtimePath) {
    if (!runtimePath || !runtimePath[0]) return NULL;
    if (!g_mesh_preview_cache.populated ||
        strcmp(g_mesh_preview_cache.runtimePath, runtimePath) != 0) {
        char diagnostics[160];
        (void)MeshPreview_LoadRuntimePath(runtimePath,
                                          &g_mesh_preview_cache,
                                          diagnostics,
                                          sizeof(diagnostics));
    }
    return g_mesh_preview_cache.valid ? &g_mesh_preview_cache : NULL;
}

bool Layout_MeshRuntimePreview_LoadStats(const char* runtimePath,
                                         LayoutMeshRuntimePreviewStats* outStats,
                                         char* diagnostics,
                                         size_t diagnosticsSize) {
    LayoutMeshRuntimePreviewCache temp = {0};
    if (outStats) memset(outStats, 0, sizeof(*outStats));
    if (!MeshPreview_LoadRuntimePath(runtimePath, &temp, diagnostics, diagnosticsSize)) {
        return false;
    }
    if (outStats) *outStats = temp.stats;
    return true;
}

bool Layout_RenderMeshAssetInstanceWireframe(SDL_Renderer* renderer,
                                             const Object3D* object,
                                             const SpaceViewContext* viewCtx,
                                             const Grid* grid,
                                             bool selected,
                                             bool hovered,
                                             float depthFactor) {
    const LayoutMeshRuntimePreviewCache* cache = NULL;
    Uint8 r = 154u;
    Uint8 g = 202u;
    Uint8 b = 250u;
    Uint8 a = 210u;
    if (!renderer || !object || !viewCtx || !grid) return false;
    if (object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) return false;
    cache = MeshPreview_CacheForPath(object->meshInstance.runtimePath);
    if (!cache || cache->stats.edgeCount == 0u) return false;

    if (selected) {
        r = 255u;
        g = 212u;
        b = 76u;
        a = 255u;
    } else if (hovered) {
        r = 94u;
        g = 228u;
        b = 255u;
        a = 245u;
    } else {
        r = (Uint8)SDL_max(30, SDL_min(255, (int)lroundf((float)r * depthFactor)));
        g = (Uint8)SDL_max(30, SDL_min(255, (int)lroundf((float)g * depthFactor)));
        b = (Uint8)SDL_max(30, SDL_min(255, (int)lroundf((float)b * depthFactor)));
    }

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    for (size_t i = 0u; i < cache->stats.edgeCount; ++i) {
        const Vec3 worldA = MeshPreview_TransformPoint(object, cache->edges[i].a);
        const Vec3 worldB = MeshPreview_TransformPoint(object, cache->edges[i].b);
        const Vec2 screenA = WorldToScreen(SpaceAdapter_ProjectToView(worldA, viewCtx), grid);
        const Vec2 screenB = WorldToScreen(SpaceAdapter_ProjectToView(worldB, viewCtx), grid);
        SDL_RenderDrawLine(renderer,
                           (int)lroundf(screenA.x),
                           (int)lroundf(screenA.y),
                           (int)lroundf(screenB.x),
                           (int)lroundf(screenB.y));
    }
    return true;
}
