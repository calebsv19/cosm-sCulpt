#pragma once

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"

#include <SDL2/SDL.h>
#include <stddef.h>

typedef struct {
    char previewMode[32];
    char sampleStrategy[48];
    size_t sourceVertexCount;
    size_t loadedVertexCount;
    size_t sourceTriangleCount;
    size_t previewVertexCount;
    size_t previewEdgeCount;
    size_t previewTriangleCount;
    size_t maxBudget;
    size_t edgeCount;
    double coverageRatio;
    double maxSpan;
    double boundingSphereRadius;
    Vec3 localBoundsMin;
    Vec3 localBoundsMax;
    Vec3 boundsCenter;
    Vec3 boundsExtent;
    bool hasDrawablePayload;
    bool metadataOnly;
    bool vertexTruncated;
    bool triangleSampled;
} LayoutMeshRuntimePreviewStats;

bool Layout_MeshRuntimePreview_LoadStats(const char* runtimePath,
                                         LayoutMeshRuntimePreviewStats* outStats,
                                         char* diagnostics,
                                         size_t diagnosticsSize);

bool Layout_RenderMeshAssetInstanceWireframe(SDL_Renderer* renderer,
                                             const Object3D* object,
                                             const SpaceViewContext* viewCtx,
                                             const Grid* grid,
                                             bool selected,
                                             bool hovered,
                                             float depthFactor);
