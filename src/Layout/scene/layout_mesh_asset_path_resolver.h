#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LAYOUT_MESH_PATH_MISSING = 0,
    LAYOUT_MESH_PATH_EXACT = 1,
    LAYOUT_MESH_PATH_RELOCATED = 2
} LayoutMeshPathResolution;

// Resolve a stored runtime-mesh path without mutating the owning scene document.
LayoutMeshPathResolution Layout_MeshAssetResolveRuntimePath(const char* storedPath,
                                                            char* resolvedPath,
                                                            size_t resolvedPathSize);

// Prefer an attachment owned by the active canonical scene bundle before
// falling back to the stored path and configured library roots.
LayoutMeshPathResolution Layout_MeshAssetResolveRuntimePathForScene(
    const char* storedPath,
    const char* sceneAuthoringPath,
    char* resolvedPath,
    size_t resolvedPathSize);
