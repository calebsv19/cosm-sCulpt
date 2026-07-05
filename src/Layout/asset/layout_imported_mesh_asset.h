#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_compile.h"

typedef enum {
    LAYOUT_IMPORTED_MESH_STL_CACHE_MISSING = 0,
    LAYOUT_IMPORTED_MESH_STL_CACHE_FRESH,
    LAYOUT_IMPORTED_MESH_STL_CACHE_STALE
} LayoutImportedMeshStlCacheState;

bool LayoutImportedMeshAsset_ResolveStlOutputPaths(const char* stl_path,
                                                   const char* asset_root,
                                                   char* out_authoring_path,
                                                   size_t out_authoring_path_size,
                                                   char* out_runtime_path,
                                                   size_t out_runtime_path_size,
                                                   char* out_preview_path,
                                                   size_t out_preview_path_size,
                                                   char* diagnostics,
                                                   size_t diagnostics_size);
LayoutImportedMeshStlCacheState LayoutImportedMeshAsset_GetStlCacheState(
    const char* stl_path,
    const char* asset_root,
    char* out_authoring_path,
    size_t out_authoring_path_size,
    char* out_runtime_path,
    size_t out_runtime_path_size,
    char* out_preview_path,
    size_t out_preview_path_size,
    char* diagnostics,
    size_t diagnostics_size);

bool LayoutImportedMeshAsset_ImportStlToRuntime(const char* stl_path,
                                                const char* asset_root,
                                                char* out_authoring_path,
                                                size_t out_authoring_path_size,
                                                char* out_runtime_path,
                                                size_t out_runtime_path_size,
                                                char* diagnostics,
                                                size_t diagnostics_size);
bool LayoutImportedMeshAsset_ImportStlToRuntimeWithProgress(
    const char* stl_path,
    const char* asset_root,
    char* out_authoring_path,
    size_t out_authoring_path_size,
    char* out_runtime_path,
    size_t out_runtime_path_size,
    char* diagnostics,
    size_t diagnostics_size,
    CoreMeshCompileProgressCallback progress_callback,
    void* progress_user_data);
