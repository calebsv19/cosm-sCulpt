#include "Layout/scene/layout_mesh_preview_sidecar.h"

#include "core_mesh_preview.h"

#include <stdio.h>
#include <string.h>

static void MeshPreviewSidecar_SetDiagnostics(char* diagnostics,
                                              size_t diagnosticsSize,
                                              const char* message) {
    if (!diagnostics || diagnosticsSize == 0u) return;
    snprintf(diagnostics, diagnosticsSize, "%s", message ? message : "");
}

static bool MeshPreviewSidecar_CoreResultToBool(CoreResult result,
                                                char* diagnostics,
                                                size_t diagnosticsSize) {
    if (result.code == CORE_OK) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);
        return true;
    }
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, result.message);
    return false;
}

static Vec3 MeshPreviewSidecar_CoreVecToVec3(CoreObjectVec3 v) {
    return (Vec3){ (float)v.x, (float)v.y, (float)v.z };
}

static void MeshPreviewSidecar_CopyMetadata(
    const CoreMeshPreviewRuntimeMetadata* source,
    LayoutMeshPreviewSidecarMetadata* target) {
    if (!source || !target) return;
    memset(target, 0, sizeof(*target));
    snprintf(target->previewMode,
             sizeof(target->previewMode),
             "%s",
             core_mesh_preview_mode_name(source->mode));
    snprintf(target->sampleStrategy,
             sizeof(target->sampleStrategy),
             "%s",
             core_mesh_preview_sample_strategy_name(source->sample_strategy));
    target->localBoundsMin = MeshPreviewSidecar_CoreVecToVec3(source->local_bounds.min);
    target->localBoundsMax = MeshPreviewSidecar_CoreVecToVec3(source->local_bounds.max);
    target->boundsCenter = MeshPreviewSidecar_CoreVecToVec3(source->bounds_center);
    target->boundsExtent = MeshPreviewSidecar_CoreVecToVec3(source->bounds_extent);
    target->sourceVertexCount = source->source_vertex_count;
    target->sourceTriangleCount = source->source_triangle_count;
    target->previewVertexCount = source->preview_vertex_count;
    target->previewEdgeCount = source->preview_edge_count;
    target->previewTriangleCount = source->preview_triangle_count;
    target->maxBudget = source->max_budget;
    target->edgeCount = source->edge_count;
    target->coverageRatio = source->coverage_ratio;
    target->maxSpan = source->max_span;
    target->boundingSphereRadius = source->bounding_sphere_radius;
    target->hasDrawablePayload = source->has_drawable_payload;
}

bool Layout_MeshPreviewSidecarPathFromRuntime(const char* runtimePath,
                                              char* outPath,
                                              size_t outPathSize) {
    return core_mesh_preview_path_from_runtime(runtimePath, outPath, outPathSize).code == CORE_OK;
}

bool Layout_MeshPreviewSidecarWriteRuntimeDocument(
    const CoreMeshAssetRuntimeDocument* document,
    const char* runtimePath,
    char* diagnostics,
    size_t diagnosticsSize) {
    return MeshPreviewSidecar_CoreResultToBool(
        core_mesh_preview_save_for_runtime_document(document,
                                                    runtimePath,
                                                    LD_MESH_PREVIEW_MAX_EDGES,
                                                    NULL,
                                                    0u),
        diagnostics,
        diagnosticsSize);
}

bool Layout_MeshPreviewSidecarReadInstance(const char* runtimePath,
                                           MeshAssetInstance3D* outInstance,
                                           char* diagnostics,
                                           size_t diagnosticsSize) {
    CoreMeshPreviewRuntimePayload payload;
    char previewPath[512];
    CoreResult result;
    bool ok = false;

    if (!outInstance) return false;
    core_mesh_preview_runtime_payload_init(&payload);
    result = core_mesh_preview_path_from_runtime(runtimePath, previewPath, sizeof(previewPath));
    if (result.code == CORE_OK) {
        result = core_mesh_preview_load_file(previewPath, &payload);
    }
    if (result.code != CORE_OK) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, result.message);
        goto cleanup;
    }

    memset(outInstance, 0, sizeof(*outInstance));
    snprintf(outInstance->assetId, sizeof(outInstance->assetId), "%s", payload.asset_id);
    snprintf(outInstance->sourceAssetId,
             sizeof(outInstance->sourceAssetId),
             "%s",
             payload.source_asset_id);
    snprintf(outInstance->runtimePath, sizeof(outInstance->runtimePath), "%s", runtimePath);
    outInstance->localBoundsMin = MeshPreviewSidecar_CoreVecToVec3(payload.local_bounds.min);
    outInstance->localBoundsMax = MeshPreviewSidecar_CoreVecToVec3(payload.local_bounds.max);
    outInstance->vertexCount = payload.source_vertex_count;
    outInstance->triangleCount = payload.source_triangle_count;
    outInstance->lockToBounds = true;
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);
    ok = true;

cleanup:
    core_mesh_preview_runtime_payload_free(&payload);
    return ok;
}

bool Layout_MeshPreviewSidecarReadMetadata(const char* runtimePath,
                                           LayoutMeshPreviewSidecarMetadata* outMetadata,
                                           char* diagnostics,
                                           size_t diagnosticsSize) {
    CoreMeshPreviewRuntimeMetadata metadata;
    char previewPath[512];
    CoreResult result;
    if (!outMetadata) return false;
    core_mesh_preview_runtime_metadata_init(&metadata);
    result = core_mesh_preview_path_from_runtime(runtimePath, previewPath, sizeof(previewPath));
    if (result.code == CORE_OK) {
        result = core_mesh_preview_load_metadata_only(previewPath, &metadata);
    }
    if (result.code != CORE_OK) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, result.message);
        return false;
    }
    MeshPreviewSidecar_CopyMetadata(&metadata, outMetadata);
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);
    return true;
}

bool Layout_MeshPreviewSidecarReadEdges(const char* runtimePath,
                                        LayoutMeshPreviewSidecarEdge* outEdges,
                                        size_t edgeCapacity,
                                        size_t* outEdgeCount,
                                        size_t* outSourceVertexCount,
                                        size_t* outSourceTriangleCount,
                                        size_t* outPreviewVertexCount,
                                        size_t* outPreviewEdgeCount,
                                        size_t* outPreviewTriangleCount,
                                        size_t* outMaxBudget,
                                        double* outCoverageRatio,
                                        double* outMaxSpan,
                                        double* outBoundingSphereRadius,
                                        char* diagnostics,
                                        size_t diagnosticsSize) {
    CoreMeshPreviewRuntimePayload payload;
    char previewPath[512];
    CoreResult result;
    size_t copied = 0u;
    bool ok = false;

    if (outEdgeCount) *outEdgeCount = 0u;
    if (outSourceVertexCount) *outSourceVertexCount = 0u;
    if (outSourceTriangleCount) *outSourceTriangleCount = 0u;
    if (outPreviewVertexCount) *outPreviewVertexCount = 0u;
    if (outPreviewEdgeCount) *outPreviewEdgeCount = 0u;
    if (outPreviewTriangleCount) *outPreviewTriangleCount = 0u;
    if (outMaxBudget) *outMaxBudget = 0u;
    if (outCoverageRatio) *outCoverageRatio = 0.0;
    if (outMaxSpan) *outMaxSpan = 0.0;
    if (outBoundingSphereRadius) *outBoundingSphereRadius = 0.0;
    if (!outEdges || edgeCapacity == 0u) return false;

    core_mesh_preview_runtime_payload_init(&payload);
    result = core_mesh_preview_path_from_runtime(runtimePath, previewPath, sizeof(previewPath));
    if (result.code == CORE_OK) {
        result = core_mesh_preview_load_file(previewPath, &payload);
    }
    if (result.code != CORE_OK) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, result.message);
        goto cleanup;
    }

    while (copied < payload.edge_count && copied < edgeCapacity) {
        outEdges[copied] = (LayoutMeshPreviewSidecarEdge){
            MeshPreviewSidecar_CoreVecToVec3(payload.edges[copied].a),
            MeshPreviewSidecar_CoreVecToVec3(payload.edges[copied].b)
        };
        ++copied;
    }
    if (copied == 0u) {
        MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, "mesh preview sidecar loaded no edges");
        goto cleanup;
    }

    if (outEdgeCount) *outEdgeCount = copied;
    if (outSourceVertexCount) *outSourceVertexCount = payload.source_vertex_count;
    if (outSourceTriangleCount) *outSourceTriangleCount = payload.source_triangle_count;
    if (outPreviewVertexCount) *outPreviewVertexCount = payload.preview_vertex_count;
    if (outPreviewEdgeCount) *outPreviewEdgeCount = payload.preview_edge_count;
    if (outPreviewTriangleCount) *outPreviewTriangleCount = payload.preview_triangle_count;
    if (outMaxBudget) *outMaxBudget = payload.max_budget;
    if (outCoverageRatio) *outCoverageRatio = payload.coverage_ratio;
    if (outMaxSpan) *outMaxSpan = payload.max_span;
    if (outBoundingSphereRadius) *outBoundingSphereRadius = payload.bounding_sphere_radius;
    MeshPreviewSidecar_SetDiagnostics(diagnostics, diagnosticsSize, NULL);
    ok = true;

cleanup:
    core_mesh_preview_runtime_payload_free(&payload);
    return ok;
}
