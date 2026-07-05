#include "Layout/asset/layout_imported_mesh_asset.h"

#include "Core/data_paths.h"
#include "Layout/scene/layout_mesh_preview_sidecar.h"
#include "core_mesh_asset.h"
#include "core_mesh_compile.h"
#include "core_units.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

static void LayoutImportedMesh_SetDiagnostics(char* diagnostics,
                                              size_t diagnostics_size,
                                              const char* message) {
    if (!diagnostics || diagnostics_size == 0u) return;
    snprintf(diagnostics, diagnostics_size, "%s", message ? message : "");
}

static bool LayoutImportedMesh_CopyText(char* dst, size_t dst_size, const char* src) {
    size_t len = 0u;
    if (!dst || dst_size == 0u || !src || !src[0]) return false;
    len = strlen(src);
    if (len >= dst_size) return false;
    memcpy(dst, src, len + 1u);
    return true;
}

static bool LayoutImportedMesh_PathHasStlSuffix(const char* path) {
    size_t len = 0u;
    if (!path || !path[0]) return false;
    len = strlen(path);
    return len > 4u && strcasecmp(path + len - 4u, ".stl") == 0;
}

static const char* LayoutImportedMesh_BaseName(const char* path) {
    const char* slash = NULL;
    if (!path || !path[0]) return "imported_stl";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void LayoutImportedMesh_BuildAssetId(const char* stl_path,
                                            char* out_asset_id,
                                            size_t out_asset_id_size) {
    const char* base = LayoutImportedMesh_BaseName(stl_path);
    size_t len = strlen(base);
    size_t out = 0u;
    if (!out_asset_id || out_asset_id_size == 0u) return;
    out_asset_id[0] = '\0';
    if (len > 4u && strcasecmp(base + len - 4u, ".stl") == 0) {
        len -= 4u;
    }
    if (out_asset_id_size > 10u) {
        memcpy(out_asset_id, "imported_", 9u);
        out = 9u;
    }
    for (size_t i = 0u; i < len && out + 1u < out_asset_id_size; ++i) {
        unsigned char ch = (unsigned char)base[i];
        if (isalnum(ch)) {
            out_asset_id[out++] = (char)tolower(ch);
        } else if (out > 0u && out_asset_id[out - 1u] != '_') {
            out_asset_id[out++] = '_';
        }
    }
    while (out > 0u && out_asset_id[out - 1u] == '_') --out;
    out_asset_id[out] = '\0';
    if (!out_asset_id[0]) {
        snprintf(out_asset_id, out_asset_id_size, "imported_stl");
    }
}

static bool LayoutImportedMesh_ResolveSourcePath(const char* path,
                                                 char* out_path,
                                                 size_t out_path_size) {
    char resolved[PATH_MAX];
    if (!path || !path[0] || !out_path || out_path_size == 0u) return false;
    if (realpath(path, resolved)) {
        return LayoutImportedMesh_CopyText(out_path, out_path_size, resolved);
    }
    return LayoutImportedMesh_CopyText(out_path, out_path_size, path);
}

static bool LayoutImportedMesh_BuildOutputPaths(const char* asset_root,
                                                const char* asset_id,
                                                char* out_authoring_path,
                                                size_t out_authoring_path_size,
                                                char* out_runtime_path,
                                                size_t out_runtime_path_size) {
    char filename[160];
    if (!asset_root || !asset_root[0] || !asset_id || !asset_id[0]) return false;
    if (!out_authoring_path || out_authoring_path_size == 0u ||
        !out_runtime_path || out_runtime_path_size == 0u) {
        return false;
    }
    if (snprintf(filename, sizeof(filename), "%s.json", asset_id) >= (int)sizeof(filename)) {
        return false;
    }
    if (!LineDrawingDataPaths_BuildPath(out_authoring_path,
                                        out_authoring_path_size,
                                        asset_root,
                                        filename)) {
        return false;
    }
    if (snprintf(filename, sizeof(filename), "%s.runtime.json", asset_id) >=
        (int)sizeof(filename)) {
        return false;
    }
    return LineDrawingDataPaths_BuildPath(out_runtime_path,
                                          out_runtime_path_size,
                                          asset_root,
                                          filename);
}

bool LayoutImportedMeshAsset_ResolveStlOutputPaths(const char* stl_path,
                                                   const char* asset_root,
                                                   char* out_authoring_path,
                                                   size_t out_authoring_path_size,
                                                   char* out_runtime_path,
                                                   size_t out_runtime_path_size,
                                                   char* out_preview_path,
                                                   size_t out_preview_path_size,
                                                   char* diagnostics,
                                                   size_t diagnostics_size) {
    char asset_id[64];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];
    if (out_authoring_path && out_authoring_path_size > 0u) out_authoring_path[0] = '\0';
    if (out_runtime_path && out_runtime_path_size > 0u) out_runtime_path[0] = '\0';
    if (out_preview_path && out_preview_path_size > 0u) out_preview_path[0] = '\0';
    LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, NULL);

    if (!stl_path || !stl_path[0] || !asset_root || !asset_root[0]) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "missing STL path or asset root");
        return false;
    }
    if (!LayoutImportedMesh_PathHasStlSuffix(stl_path)) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "selected file is not an STL");
        return false;
    }

    LayoutImportedMesh_BuildAssetId(stl_path, asset_id, sizeof(asset_id));
    if (!LayoutImportedMesh_BuildOutputPaths(asset_root,
                                             asset_id,
                                             authoring_path,
                                             sizeof(authoring_path),
                                             runtime_path,
                                             sizeof(runtime_path))) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "failed to build import output paths");
        return false;
    }
    if (!Layout_MeshPreviewSidecarPathFromRuntime(runtime_path,
                                                  preview_path,
                                                  sizeof(preview_path))) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "failed to build preview output path");
        return false;
    }

    if (out_authoring_path && out_authoring_path_size > 0u) {
        snprintf(out_authoring_path, out_authoring_path_size, "%s", authoring_path);
    }
    if (out_runtime_path && out_runtime_path_size > 0u) {
        snprintf(out_runtime_path, out_runtime_path_size, "%s", runtime_path);
    }
    if (out_preview_path && out_preview_path_size > 0u) {
        snprintf(out_preview_path, out_preview_path_size, "%s", preview_path);
    }
    return true;
}

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
    size_t diagnostics_size) {
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];
    struct stat stl_stat = {0};
    struct stat authoring_stat = {0};
    struct stat runtime_stat = {0};
    struct stat preview_stat = {0};

    if (!LayoutImportedMeshAsset_ResolveStlOutputPaths(stl_path,
                                                       asset_root,
                                                       authoring_path,
                                                       sizeof(authoring_path),
                                                       runtime_path,
                                                       sizeof(runtime_path),
                                                       preview_path,
                                                       sizeof(preview_path),
                                                       diagnostics,
                                                       diagnostics_size)) {
        return LAYOUT_IMPORTED_MESH_STL_CACHE_MISSING;
    }
    if (out_authoring_path && out_authoring_path_size > 0u) {
        snprintf(out_authoring_path, out_authoring_path_size, "%s", authoring_path);
    }
    if (out_runtime_path && out_runtime_path_size > 0u) {
        snprintf(out_runtime_path, out_runtime_path_size, "%s", runtime_path);
    }
    if (out_preview_path && out_preview_path_size > 0u) {
        snprintf(out_preview_path, out_preview_path_size, "%s", preview_path);
    }

    if (stat(stl_path, &stl_stat) != 0 || !S_ISREG(stl_stat.st_mode)) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "STL source is missing");
        return LAYOUT_IMPORTED_MESH_STL_CACHE_MISSING;
    }
    if (stat(authoring_path, &authoring_stat) != 0 ||
        !S_ISREG(authoring_stat.st_mode) ||
        stat(runtime_path, &runtime_stat) != 0 ||
        !S_ISREG(runtime_stat.st_mode) ||
        stat(preview_path, &preview_stat) != 0 ||
        !S_ISREG(preview_stat.st_mode)) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "STL runtime cache is missing");
        return LAYOUT_IMPORTED_MESH_STL_CACHE_MISSING;
    }
    if (authoring_stat.st_mtime < stl_stat.st_mtime ||
        runtime_stat.st_mtime < stl_stat.st_mtime ||
        preview_stat.st_mtime < stl_stat.st_mtime) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "STL runtime cache is stale");
        return LAYOUT_IMPORTED_MESH_STL_CACHE_STALE;
    }

    LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    return LAYOUT_IMPORTED_MESH_STL_CACHE_FRESH;
}

static bool LayoutImportedMesh_PopulateAuthoringDocument(
    const char* stl_path,
    const char* asset_id,
    CoreMeshAssetAuthoringDocument* document,
    char* diagnostics,
    size_t diagnostics_size) {
    char source_path[PATH_MAX];
    CoreResult result;
    if (!stl_path || !asset_id || !document) return false;

    core_mesh_asset_authoring_document_init(document);
    result = core_mesh_asset_authoring_contract_set_asset_id(&document->contract, asset_id);
    if (result.code != CORE_OK) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, result.message);
        return false;
    }
    document->contract.unit_kind = CORE_UNIT_METER;
    document->contract.world_scale = 1.0;
    document->contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    document->contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH;
    document->contract.topology_closed_volume_expected = true;
    document->contract.topology_manifold_expected = true;

    document->has_imported_mesh_source = true;
    core_mesh_asset_imported_mesh_source_init(&document->imported_mesh_source);
    if (!LayoutImportedMesh_ResolveSourcePath(stl_path, source_path, sizeof(source_path)) ||
        !LayoutImportedMesh_CopyText(document->imported_mesh_source.import_id,
                                     sizeof(document->imported_mesh_source.import_id),
                                     "import_stl_01") ||
        !LayoutImportedMesh_CopyText(document->imported_mesh_source.source_uri,
                                     sizeof(document->imported_mesh_source.source_uri),
                                     source_path) ||
        !LayoutImportedMesh_CopyText(document->imported_mesh_source.orientation_policy,
                                     sizeof(document->imported_mesh_source.orientation_policy),
                                     "source_axes") ||
        !LayoutImportedMesh_CopyText(
            document->imported_mesh_source.default_surface_group_id,
            sizeof(document->imported_mesh_source.default_surface_group_id),
            "imported_surface")) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "invalid STL import metadata");
        return false;
    }
    document->imported_mesh_source.source_format =
        CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL;
    document->imported_mesh_source.source_unit_kind = CORE_UNIT_METER;
    document->imported_mesh_source.source_to_asset_scale = 1.0;
    document->imported_mesh_source.weld_vertices = true;
    document->imported_mesh_source.weld_tolerance = 0.000001;
    document->imported_mesh_source.preserve_source_normals = false;
    document->imported_mesh_source.topology_closed_volume_observed = true;
    document->imported_mesh_source.topology_manifold_observed = true;

    result = core_mesh_asset_authoring_document_validate(document);
    if (result.code != CORE_OK) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, result.message);
        return false;
    }
    LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    return true;
}

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
    void* progress_user_data) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshAssetRuntimeDocument runtime_document;
    CoreResult result;
    char asset_id[64];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    bool ok = false;

    if (out_authoring_path && out_authoring_path_size > 0u) out_authoring_path[0] = '\0';
    if (out_runtime_path && out_runtime_path_size > 0u) out_runtime_path[0] = '\0';
    LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    core_mesh_asset_authoring_document_init(&document);
    core_mesh_asset_runtime_document_init(&runtime_document);

    if (!stl_path || !stl_path[0] || !asset_root || !asset_root[0]) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "missing STL path or asset root");
        return false;
    }
    if (!LayoutImportedMesh_PathHasStlSuffix(stl_path)) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "selected file is not an STL");
        return false;
    }

    LayoutImportedMesh_BuildAssetId(stl_path, asset_id, sizeof(asset_id));
    if (!LayoutImportedMesh_BuildOutputPaths(asset_root,
                                             asset_id,
                                             authoring_path,
                                             sizeof(authoring_path),
                                             runtime_path,
                                             sizeof(runtime_path))) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, "failed to build import output paths");
        return false;
    }
    if (!LayoutImportedMesh_PopulateAuthoringDocument(stl_path,
                                                      asset_id,
                                                      &document,
                                                      diagnostics,
                                                      diagnostics_size)) {
        goto done;
    }

    result = core_mesh_asset_authoring_document_save_file(&document, authoring_path);
    if (result.code != CORE_OK) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, result.message);
        goto done;
    }
    result = core_mesh_compile_imported_mesh_to_runtime_document_with_progress(&document,
                                                                              NULL,
                                                                              asset_id,
                                                                              &runtime_document,
                                                                              progress_callback,
                                                                              progress_user_data);
    if (result.code != CORE_OK) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, result.message);
        goto done;
    }
    result = core_mesh_asset_runtime_document_save_file(&runtime_document, runtime_path);
    if (result.code != CORE_OK) {
        LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, result.message);
        goto done;
    }
    if (!Layout_MeshPreviewSidecarWriteRuntimeDocument(&runtime_document,
                                                       runtime_path,
                                                       diagnostics,
                                                       diagnostics_size)) {
        goto done;
    }

    if (out_authoring_path && out_authoring_path_size > 0u) {
        snprintf(out_authoring_path, out_authoring_path_size, "%s", authoring_path);
    }
    if (out_runtime_path && out_runtime_path_size > 0u) {
        snprintf(out_runtime_path, out_runtime_path_size, "%s", runtime_path);
    }
    LayoutImportedMesh_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    ok = true;

done:
    core_mesh_asset_runtime_document_free(&runtime_document);
    core_mesh_asset_authoring_document_free(&document);
    return ok;
}

bool LayoutImportedMeshAsset_ImportStlToRuntime(const char* stl_path,
                                                const char* asset_root,
                                                char* out_authoring_path,
                                                size_t out_authoring_path_size,
                                                char* out_runtime_path,
                                                size_t out_runtime_path_size,
                                                char* diagnostics,
                                                size_t diagnostics_size) {
    return LayoutImportedMeshAsset_ImportStlToRuntimeWithProgress(stl_path,
                                                                  asset_root,
                                                                  out_authoring_path,
                                                                  out_authoring_path_size,
                                                                  out_runtime_path,
                                                                  out_runtime_path_size,
                                                                  diagnostics,
                                                                  diagnostics_size,
                                                                  NULL,
                                                                  NULL);
}
