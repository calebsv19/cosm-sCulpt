#include "Layout/scene/layout_mesh_preview_sidecar.h"

#include "core_io.h"

#include "cjson/cJSON.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LD_MESH_PREVIEW_SHARP_EDGE_DOT_THRESHOLD 0.8191520443f

typedef struct {
    size_t a;
    size_t b;
    Vec3 normal;
} MeshPreviewTempEdge;

typedef struct {
    size_t a;
    size_t b;
} MeshPreviewEdgeKey;

static void MeshPreviewSidecar_SetDiagnostics(char* diagnostics,
                                              size_t diagnosticsSize,
                                              const char* message) {
    if (!diagnostics || diagnosticsSize == 0u) return;
    snprintf(diagnostics, diagnosticsSize, "%s", message ? message : "");
}

static bool MeshPreviewSidecar_WriteJsonString(FILE* f, const char* text) {
    const unsigned char* p = (const unsigned char*)(text ? text : "");
    if (fputc('"', f) == EOF) return false;
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
            case '\\': if (fputs("\\\\", f) == EOF) return false; break;
            case '"': if (fputs("\\\"", f) == EOF) return false; break;
            case '\b': if (fputs("\\b", f) == EOF) return false; break;
            case '\f': if (fputs("\\f", f) == EOF) return false; break;
            case '\n': if (fputs("\\n", f) == EOF) return false; break;
            case '\r': if (fputs("\\r", f) == EOF) return false; break;
            case '\t': if (fputs("\\t", f) == EOF) return false; break;
            default:
                if (ch < 0x20u) {
                    if (fprintf(f, "\\u%04x", (unsigned int)ch) < 0) return false;
                } else {
                    if (fputc((int)ch, f) == EOF) return false;
                }
                break;
        }
    }
    return fputc('"', f) != EOF;
}

static bool MeshPreviewSidecar_WriteVec3(FILE* f, Vec3 v) {
    return fprintf(f, "{\"x\":%.9g,\"y\":%.9g,\"z\":%.9g}", v.x, v.y, v.z) >= 0;
}

static Vec3 MeshPreviewSidecar_CoreVecToVec3(CoreObjectVec3 v) {
    return (Vec3){ (float)v.x, (float)v.y, (float)v.z };
}

static Vec3 MeshPreviewSidecar_Sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

static Vec3 MeshPreviewSidecar_Cross(Vec3 a, Vec3 b) {
    return (Vec3){
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)
    };
}

static float MeshPreviewSidecar_Dot(Vec3 a, Vec3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

static Vec3 MeshPreviewSidecar_Normalize(Vec3 v) {
    const float lenSq = MeshPreviewSidecar_Dot(v, v);
    if (lenSq <= 0.0f || !isfinite(lenSq)) return (Vec3){0};
    {
        const float invLen = 1.0f / sqrtf(lenSq);
        return (Vec3){ v.x * invLen, v.y * invLen, v.z * invLen };
    }
}

static Vec3 MeshPreviewSidecar_TriangleNormal(const CoreMeshAssetRuntimeDocument* document,
                                              const CoreMeshAssetRuntimeTriangle* tri) {
    const Vec3 a = MeshPreviewSidecar_CoreVecToVec3(document->vertices[tri->a].position);
    const Vec3 b = MeshPreviewSidecar_CoreVecToVec3(document->vertices[tri->b].position);
    const Vec3 c = MeshPreviewSidecar_CoreVecToVec3(document->vertices[tri->c].position);
    return MeshPreviewSidecar_Normalize(
        MeshPreviewSidecar_Cross(MeshPreviewSidecar_Sub(b, a),
                                 MeshPreviewSidecar_Sub(c, a)));
}

static int MeshPreviewSidecar_CompareTempEdges(const void* lhs, const void* rhs) {
    const MeshPreviewTempEdge* a = (const MeshPreviewTempEdge*)lhs;
    const MeshPreviewTempEdge* b = (const MeshPreviewTempEdge*)rhs;
    if (a->a < b->a) return -1;
    if (a->a > b->a) return 1;
    if (a->b < b->b) return -1;
    if (a->b > b->b) return 1;
    return 0;
}

static void MeshPreviewSidecar_AddTempEdge(MeshPreviewTempEdge* edges,
                                           size_t* edgeCount,
                                           size_t a,
                                           size_t b,
                                           Vec3 normal) {
    MeshPreviewTempEdge* edge = NULL;
    if (!edges || !edgeCount) return;
    edge = &edges[(*edgeCount)++];
    if (a < b) {
        edge->a = a;
        edge->b = b;
    } else {
        edge->a = b;
        edge->b = a;
    }
    edge->normal = normal;
}

static bool MeshPreviewSidecar_BuildFeatureEdges(
    const CoreMeshAssetRuntimeDocument* document,
    MeshPreviewEdgeKey** outEdges,
    size_t* outEdgeCount,
    char* diagnostics,
    size_t diagnosticsSize) {
    MeshPreviewTempEdge* tempEdges = NULL;
    MeshPreviewEdgeKey* candidates = NULL;
    size_t tempEdgeCount = 0u;
    size_t candidateCount = 0u;
    size_t fallbackCount = 0u;
    bool ok = false;

    if (outEdges) *outEdges = NULL;
    if (outEdgeCount) *outEdgeCount = 0u;
    if (!document || !outEdges || !outEdgeCount) return false;
    if (document->triangle_count == 0u ||
        document->triangle_count > SIZE_MAX / (3u * sizeof(MeshPreviewTempEdge))) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "invalid mesh preview triangle count");
        return false;
    }

    tempEdges = (MeshPreviewTempEdge*)malloc(document->triangle_count * 3u *
                                            sizeof(MeshPreviewTempEdge));
    candidates = (MeshPreviewEdgeKey*)malloc(document->triangle_count * 3u *
                                             sizeof(MeshPreviewEdgeKey));
    if (!tempEdges || !candidates) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to allocate mesh preview edge set");
        goto cleanup;
    }

    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle* tri = &document->triangles[i];
        const Vec3 normal = MeshPreviewSidecar_TriangleNormal(document, tri);
        MeshPreviewSidecar_AddTempEdge(tempEdges, &tempEdgeCount, tri->a, tri->b, normal);
        MeshPreviewSidecar_AddTempEdge(tempEdges, &tempEdgeCount, tri->b, tri->c, normal);
        MeshPreviewSidecar_AddTempEdge(tempEdges, &tempEdgeCount, tri->c, tri->a, normal);
    }

    qsort(tempEdges, tempEdgeCount, sizeof(tempEdges[0]), MeshPreviewSidecar_CompareTempEdges);
    for (size_t i = 0u; i < tempEdgeCount;) {
        size_t j = i + 1u;
        bool feature = false;
        while (j < tempEdgeCount &&
               tempEdges[j].a == tempEdges[i].a &&
               tempEdges[j].b == tempEdges[i].b) {
            ++j;
        }

        if (j - i != 2u) {
            feature = true;
        } else {
            const float dot = MeshPreviewSidecar_Dot(tempEdges[i].normal, tempEdges[i + 1u].normal);
            feature = !isfinite(dot) || dot <= LD_MESH_PREVIEW_SHARP_EDGE_DOT_THRESHOLD;
        }
        if (feature) {
            candidates[candidateCount++] = (MeshPreviewEdgeKey){ tempEdges[i].a, tempEdges[i].b };
        }
        ++fallbackCount;
        i = j;
    }

    if (candidateCount == 0u) {
        for (size_t i = 0u; i < tempEdgeCount;) {
            size_t j = i + 1u;
            while (j < tempEdgeCount &&
                   tempEdges[j].a == tempEdges[i].a &&
                   tempEdges[j].b == tempEdges[i].b) {
                ++j;
            }
            candidates[candidateCount++] = (MeshPreviewEdgeKey){ tempEdges[i].a, tempEdges[i].b };
            i = j;
        }
    }

    (void)fallbackCount;
    *outEdges = candidates;
    *outEdgeCount = candidateCount;
    candidates = NULL;
    ok = true;

cleanup:
    free(tempEdges);
    free(candidates);
    return ok;
}

static bool MeshPreviewSidecar_Vec3FromJson(const cJSON* node, Vec3* out) {
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

static bool MeshPreviewSidecar_ReadTextFile(const char* path,
                                            char** outText,
                                            char* diagnostics,
                                            size_t diagnosticsSize) {
    CoreBuffer buffer = {0};
    CoreResult readResult = {0};
    char* text = NULL;

    if (!path || !path[0] || !outText) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "missing preview sidecar path");
        return false;
    }
    *outText = NULL;
    readResult = core_io_read_all(path, &buffer);
    if (readResult.code != CORE_OK || !buffer.data || buffer.size == 0u) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to read mesh preview sidecar");
        return false;
    }
    text = (char*)malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to allocate mesh preview sidecar buffer");
        return false;
    }
    memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);
    *outText = text;
    return true;
}

static cJSON* MeshPreviewSidecar_ParseRoot(const char* runtimePath,
                                           char* diagnostics,
                                           size_t diagnosticsSize) {
    char previewPath[512];
    char* text = NULL;
    cJSON* root = NULL;
    const cJSON* schema = NULL;

    if (!Layout_MeshPreviewSidecarPathFromRuntime(runtimePath,
                                                  previewPath,
                                                  sizeof(previewPath))) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to build mesh preview sidecar path");
        return NULL;
    }
    if (!MeshPreviewSidecar_ReadTextFile(previewPath, &text, diagnostics, diagnosticsSize)) {
        return NULL;
    }
    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to parse mesh preview sidecar");
        return NULL;
    }
    schema = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    if (!cJSON_IsString(schema) ||
        strcmp(schema->valuestring, "line_drawing_mesh_runtime_preview_v1") != 0) {
        cJSON_Delete(root);
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "sidecar is not line_drawing_mesh_runtime_preview_v1");
        return NULL;
    }
    return root;
}

bool Layout_MeshPreviewSidecarPathFromRuntime(const char* runtimePath,
                                              char* outPath,
                                              size_t outPathSize) {
    const char* runtimeSuffix = ".runtime.json";
    const char* previewSuffix = ".preview.json";
    size_t runtimeLen = 0u;
    size_t suffixLen = strlen(runtimeSuffix);
    if (!runtimePath || !runtimePath[0] || !outPath || outPathSize == 0u) return false;
    runtimeLen = strlen(runtimePath);
    if (runtimeLen > suffixLen &&
        strcmp(runtimePath + runtimeLen - suffixLen, runtimeSuffix) == 0) {
        size_t baseLen = runtimeLen - suffixLen;
        if (baseLen + strlen(previewSuffix) + 1u > outPathSize) return false;
        memcpy(outPath, runtimePath, baseLen);
        memcpy(outPath + baseLen, previewSuffix, strlen(previewSuffix) + 1u);
        return true;
    }
    return snprintf(outPath, outPathSize, "%s.preview.json", runtimePath) < (int)outPathSize;
}

bool Layout_MeshPreviewSidecarWriteRuntimeDocument(
    const CoreMeshAssetRuntimeDocument* document,
    const char* runtimePath,
    char* diagnostics,
    size_t diagnosticsSize) {
    char previewPath[512];
    MeshPreviewEdgeKey* featureEdges = NULL;
    FILE* f = NULL;
    size_t stride = 1u;
    size_t sampled = 0u;
    size_t featureEdgeCount = 0u;
    size_t edgeCount = 0u;
    bool ok = false;

    if (!document || !runtimePath || !runtimePath[0]) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "missing mesh preview source");
        return false;
    }
    if (!Layout_MeshPreviewSidecarPathFromRuntime(runtimePath, previewPath, sizeof(previewPath))) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to build mesh preview sidecar path");
        return false;
    }

    if (!MeshPreviewSidecar_BuildFeatureEdges(document,
                                              &featureEdges,
                                              &featureEdgeCount,
                                              diagnostics,
                                              diagnosticsSize)) {
        return false;
    }

    if (featureEdgeCount > LD_MESH_PREVIEW_MAX_EDGES) {
        stride = (size_t)ceil((double)featureEdgeCount /
                              (double)LD_MESH_PREVIEW_MAX_EDGES);
        if (stride == 0u) stride = 1u;
    }
    for (size_t i = 0u; i < featureEdgeCount; i += stride) {
        sampled += 1u;
    }
    edgeCount = sampled;

    f = fopen(previewPath, "wb");
    if (!f) {
        free(featureEdges);
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to open mesh preview sidecar");
        return false;
    }

    ok = fprintf(f, "{\n\t\"schema_family\":\"line_drawing_mesh_preview\",\n") >= 0;
    ok = ok && fprintf(f, "\t\"schema_variant\":\"line_drawing_mesh_runtime_preview_v1\",\n") >= 0;
    ok = ok && fprintf(f, "\t\"schema_version\":1,\n\t\"asset_id\":") >= 0 &&
         MeshPreviewSidecar_WriteJsonString(f, document->contract.asset_id) &&
         fprintf(f, ",\n\t\"source_asset_id\":") >= 0 &&
         MeshPreviewSidecar_WriteJsonString(f, document->contract.source_asset_id) &&
         fprintf(f, ",\n\t\"runtime_path\":") >= 0 &&
         MeshPreviewSidecar_WriteJsonString(f, runtimePath) &&
         fprintf(f,
                 ",\n\t\"vertex_count\":%zu,\n\t\"triangle_count\":%zu,\n",
                 document->vertex_count,
                 document->triangle_count) >= 0;
    ok = ok && fprintf(f, "\t\"local_bounds\":{\"min\":") >= 0 &&
         MeshPreviewSidecar_WriteVec3(f,
             MeshPreviewSidecar_CoreVecToVec3(document->contract.local_bounds.min)) &&
         fprintf(f, ",\"max\":") >= 0 &&
         MeshPreviewSidecar_WriteVec3(f,
             MeshPreviewSidecar_CoreVecToVec3(document->contract.local_bounds.max)) &&
         fprintf(f,
                 "},\n\t\"preview_mode\":\"feature_edges_v1\",\n\t\"source_feature_edge_count\":%zu,\n\t\"sampled_triangle_count\":%zu,\n\t\"edge_count\":%zu,\n\t\"edges\":[\n",
                 featureEdgeCount,
                 sampled,
                 edgeCount) >= 0;

    sampled = 0u;
    for (size_t i = 0u; ok && i < featureEdgeCount; i += stride) {
        const MeshPreviewEdgeKey edge = featureEdges[i];
        const Vec3 a = MeshPreviewSidecar_CoreVecToVec3(document->vertices[edge.a].position);
        const Vec3 b = MeshPreviewSidecar_CoreVecToVec3(document->vertices[edge.b].position);
        ok = fprintf(f, "\t\t{\"a\":") >= 0 &&
             MeshPreviewSidecar_WriteVec3(f, a) &&
             fprintf(f, ",\"b\":") >= 0 &&
             MeshPreviewSidecar_WriteVec3(f, b) &&
             fprintf(f, "}%s\n", (++sampled < edgeCount) ? "," : "") >= 0;
    }
    ok = ok && fprintf(f, "\t]\n}\n") >= 0;
    free(featureEdges);

    if (!ok || fclose(f) != 0) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "failed to write mesh preview sidecar");
        return false;
    }
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);
    return true;
}

bool Layout_MeshPreviewSidecarReadInstance(const char* runtimePath,
                                           MeshAssetInstance3D* outInstance,
                                           char* diagnostics,
                                           size_t diagnosticsSize) {
    cJSON* root = NULL;
    const cJSON* assetId = NULL;
    const cJSON* sourceAssetId = NULL;
    const cJSON* vertexCount = NULL;
    const cJSON* triangleCount = NULL;
    const cJSON* bounds = NULL;
    Vec3 minBounds = {0};
    Vec3 maxBounds = {0};
    bool ok = false;

    if (!outInstance) return false;
    root = MeshPreviewSidecar_ParseRoot(runtimePath, diagnostics, diagnosticsSize);
    if (!root) return false;

    assetId = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    sourceAssetId = cJSON_GetObjectItemCaseSensitive(root, "source_asset_id");
    vertexCount = cJSON_GetObjectItemCaseSensitive(root, "vertex_count");
    triangleCount = cJSON_GetObjectItemCaseSensitive(root, "triangle_count");
    bounds = cJSON_GetObjectItemCaseSensitive(root, "local_bounds");

    if (!cJSON_IsString(assetId) || !assetId->valuestring || !assetId->valuestring[0] ||
        !cJSON_IsNumber(vertexCount) || vertexCount->valuedouble <= 0.0 ||
        !cJSON_IsNumber(triangleCount) || triangleCount->valuedouble <= 0.0 ||
        !cJSON_IsObject(bounds) ||
        !MeshPreviewSidecar_Vec3FromJson(cJSON_GetObjectItemCaseSensitive(bounds, "min"), &minBounds) ||
        !MeshPreviewSidecar_Vec3FromJson(cJSON_GetObjectItemCaseSensitive(bounds, "max"), &maxBounds)) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "mesh preview sidecar has invalid metadata");
        goto cleanup;
    }

    memset(outInstance, 0, sizeof(*outInstance));
    snprintf(outInstance->assetId, sizeof(outInstance->assetId), "%s", assetId->valuestring);
    if (cJSON_IsString(sourceAssetId) && sourceAssetId->valuestring) {
        snprintf(outInstance->sourceAssetId,
                 sizeof(outInstance->sourceAssetId),
                 "%s",
                 sourceAssetId->valuestring);
    }
    snprintf(outInstance->runtimePath, sizeof(outInstance->runtimePath), "%s", runtimePath);
    outInstance->localBoundsMin = minBounds;
    outInstance->localBoundsMax = maxBounds;
    outInstance->vertexCount = (size_t)vertexCount->valuedouble;
    outInstance->triangleCount = (size_t)triangleCount->valuedouble;
    outInstance->lockToBounds = true;
    ok = true;
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);

cleanup:
    cJSON_Delete(root);
    return ok;
}

bool Layout_MeshPreviewSidecarReadEdges(const char* runtimePath,
                                        LayoutMeshPreviewSidecarEdge* outEdges,
                                        size_t edgeCapacity,
                                        size_t* outEdgeCount,
                                        size_t* outSourceVertexCount,
                                        size_t* outSourceTriangleCount,
                                        size_t* outSampledTriangleCount,
                                        char* diagnostics,
                                        size_t diagnosticsSize) {
    cJSON* root = NULL;
    const cJSON* edges = NULL;
    const cJSON* vertexCount = NULL;
    const cJSON* triangleCount = NULL;
    const cJSON* sampledTriangleCount = NULL;
    int edgeArrayCount = 0;
    size_t edgeCount = 0u;
    bool ok = false;

    if (outEdgeCount) *outEdgeCount = 0u;
    if (outSourceVertexCount) *outSourceVertexCount = 0u;
    if (outSourceTriangleCount) *outSourceTriangleCount = 0u;
    if (outSampledTriangleCount) *outSampledTriangleCount = 0u;
    if (!outEdges || edgeCapacity == 0u) return false;

    root = MeshPreviewSidecar_ParseRoot(runtimePath, diagnostics, diagnosticsSize);
    if (!root) return false;

    vertexCount = cJSON_GetObjectItemCaseSensitive(root, "vertex_count");
    triangleCount = cJSON_GetObjectItemCaseSensitive(root, "triangle_count");
    sampledTriangleCount = cJSON_GetObjectItemCaseSensitive(root, "sampled_triangle_count");
    edges = cJSON_GetObjectItemCaseSensitive(root, "edges");
    if (!cJSON_IsNumber(vertexCount) || !cJSON_IsNumber(triangleCount) ||
        !cJSON_IsNumber(sampledTriangleCount) || !cJSON_IsArray(edges)) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "mesh preview sidecar has no drawable edge data");
        goto cleanup;
    }

    edgeArrayCount = cJSON_GetArraySize(edges);
    for (int i = 0; i < edgeArrayCount && edgeCount < edgeCapacity; ++i) {
        const cJSON* edge = cJSON_GetArrayItem(edges, i);
        LayoutMeshPreviewSidecarEdge parsed = {0};
        if (!MeshPreviewSidecar_Vec3FromJson(cJSON_GetObjectItemCaseSensitive(edge, "a"), &parsed.a) ||
            !MeshPreviewSidecar_Vec3FromJson(cJSON_GetObjectItemCaseSensitive(edge, "b"), &parsed.b)) {
            continue;
        }
        outEdges[edgeCount++] = parsed;
    }
    if (edgeCount == 0u) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "mesh preview sidecar loaded no edges");
        goto cleanup;
    }

    if (outEdgeCount) *outEdgeCount = edgeCount;
    if (outSourceVertexCount) *outSourceVertexCount = (size_t)vertexCount->valuedouble;
    if (outSourceTriangleCount) *outSourceTriangleCount = (size_t)triangleCount->valuedouble;
    if (outSampledTriangleCount) *outSampledTriangleCount = (size_t)sampledTriangleCount->valuedouble;
    ok = true;
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);

cleanup:
    cJSON_Delete(root);
    return ok;
}
