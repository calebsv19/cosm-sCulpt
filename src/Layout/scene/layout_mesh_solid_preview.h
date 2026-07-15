#pragma once

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"
#include "core_mesh_asset.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Coherent indexed surface used by the app-local interactive mesh preview.
typedef struct {
    Vec3* vertices;
    uint32_t* indices;
    size_t vertexCount;
    size_t triangleCount;
    size_t sourceVertexCount;
    size_t sourceTriangleCount;
    int clusterResolution;
} LayoutMeshSolidPreviewLod;

// Observable output from one software depth-buffer pass.
typedef struct {
    size_t submittedTriangles;
    size_t rasterizedTriangles;
    size_t coveredPixels;
    size_t silhouettePixels;
    size_t meshCount;
    bool interactiveQuality;
} LayoutMeshSolidPreviewFrameStats;

// App-local expression of the shared runtime mesh for each filled/outline view.
typedef enum {
    LAYOUT_MESH_SOLID_STYLE_FLAT = 0,
    LAYOUT_MESH_SOLID_STYLE_MATERIAL = 1,
    LAYOUT_MESH_SOLID_STYLE_WIRE_OUTLINE = 2
} LayoutMeshSolidPreviewStyle;

// Renderer-cache invalidation domains. Overlay-only editor feedback must never
// lower the mesh surface quality or force a surface reraster.
typedef enum {
    LAYOUT_MESH_SOLID_INVALIDATION_NONE = 0,
    LAYOUT_MESH_SOLID_INVALIDATION_APPEARANCE = 1u << 0,
    LAYOUT_MESH_SOLID_INVALIDATION_GEOMETRY = 1u << 1,
    LAYOUT_MESH_SOLID_INVALIDATION_OVERLAY = 1u << 2,
    LAYOUT_MESH_SOLID_INVALIDATION_PROJECTION = 1u << 3
} LayoutMeshSolidPreviewInvalidation;

// Build a topology-coherent vertex-cluster LOD without mutating canonical mesh data.
bool Layout_MeshSolidPreviewBuildLod(const CoreMeshAssetRuntimeDocument* document,
                                     size_t targetTriangles,
                                     LayoutMeshSolidPreviewLod* outLod);

// Release all allocations owned by one LOD surface.
void Layout_MeshSolidPreviewFreeLod(LayoutMeshSolidPreviewLod* lod);

// Add a one-pixel screen-space silhouette around covered depth-buffer pixels.
size_t Layout_MeshSolidPreviewApplySilhouette(uint8_t* rgba,
                                              const float* depth,
                                              int width,
                                              int height,
                                              uint8_t outlineR,
                                              uint8_t outlineG,
                                              uint8_t outlineB,
                                              uint8_t outlineA);

// Replace filled coverage with only its view-dependent silhouette/depth edges.
size_t Layout_MeshSolidPreviewApplyOutlineOnly(uint8_t* rgba,
                                              const float* depth,
                                              int width,
                                              int height,
                                              uint8_t outlineR,
                                              uint8_t outlineG,
                                              uint8_t outlineB,
                                              uint8_t outlineA);

// Select the reduced interaction quality until the scene signature has stayed stable.
bool Layout_MeshSolidPreviewUsesInteractiveQuality(uint64_t nowNs,
                                                   uint64_t signatureChangedAtNs);

// Only geometry/view changes restart the interactive-quality settle window.
bool Layout_MeshSolidPreviewInvalidationResetsQuality(
    LayoutMeshSolidPreviewInvalidation invalidation);

// Return true only when a view-context change alters the viewing direction.
// Camera target changes are pans, and construction-plane offset changes do not
// change the projected face orientation, so neither should lower mesh quality.
bool Layout_MeshSolidPreviewViewChangeResetsQuality(
    const SpaceViewContext* previous,
    const SpaceViewContext* current);

// Render every visible mesh instance into one cached depth-tested viewport texture.
bool Layout_RenderMeshSolidPreview(SDL_Renderer* renderer,
                                   const Layout* layout,
                                   const SpaceViewContext* viewContext,
                                   const Grid* grid,
                                   int screenWidth,
                                   int screenHeight,
                                   LayoutMeshSolidPreviewStyle style,
                                   LayoutMeshSolidPreviewFrameStats* outStats);

// Release cached mesh LODs, CPU buffers, and the uploaded viewport texture.
void Layout_MeshSolidPreviewShutdown(SDL_Renderer* renderer);
