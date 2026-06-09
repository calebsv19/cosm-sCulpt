#pragma once

#include "Layout/layout.h"
#include "core_mesh_asset.h"

#include <stddef.h>

#define LD_MESH_PREVIEW_MAX_TRIANGLES 1024u
#define LD_MESH_PREVIEW_MAX_EDGES (LD_MESH_PREVIEW_MAX_TRIANGLES * 3u)

typedef struct {
    Vec3 a;
    Vec3 b;
} LayoutMeshPreviewSidecarEdge;

bool Layout_MeshPreviewSidecarPathFromRuntime(const char* runtimePath,
                                              char* outPath,
                                              size_t outPathSize);

bool Layout_MeshPreviewSidecarWriteRuntimeDocument(
    const CoreMeshAssetRuntimeDocument* document,
    const char* runtimePath,
    char* diagnostics,
    size_t diagnosticsSize);

bool Layout_MeshPreviewSidecarReadInstance(const char* runtimePath,
                                           MeshAssetInstance3D* outInstance,
                                           char* diagnostics,
                                           size_t diagnosticsSize);

bool Layout_MeshPreviewSidecarReadEdges(const char* runtimePath,
                                        LayoutMeshPreviewSidecarEdge* outEdges,
                                        size_t edgeCapacity,
                                        size_t* outEdgeCount,
                                        size_t* outSourceVertexCount,
                                        size_t* outSourceTriangleCount,
                                        size_t* outSampledTriangleCount,
                                        char* diagnostics,
                                        size_t diagnosticsSize);
