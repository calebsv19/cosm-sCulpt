#pragma once

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"

#include <SDL2/SDL.h>
#include <stddef.h>

typedef struct {
    size_t sourceVertexCount;
    size_t loadedVertexCount;
    size_t sourceTriangleCount;
    size_t sampledTriangleCount;
    size_t edgeCount;
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
