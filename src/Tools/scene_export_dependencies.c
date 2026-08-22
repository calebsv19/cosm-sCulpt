#include "Tools/scene_export_dependencies.h"

#include "Layout/scene/layout_mesh_asset_path_resolver.h"
#include "core_io.h"
#include "core_scene_compile.h"

#include <stdio.h>
#include <string.h>

static void dependency_diagnostics(char* diagnostics, size_t size, const char* message) {
    if (diagnostics && size > 0u) snprintf(diagnostics, size, "%s", message ? message : "");
}

bool LineDrawingSceneExportDependencies_Collect(
    const Layout* layout,
    char** out_manifest_json,
    char out_digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE],
    size_t* out_dependency_count,
    char* diagnostics,
    size_t diagnostics_size) {
    CoreSceneCompileDependency* collected = NULL;
    char (*digests)[CORE_SCENE_COMPILE_SHA256_HEX_SIZE] = NULL;
    size_t capacity = 0u;
    size_t count = 0u;
    CoreResult manifest_result;

    if (!layout || !out_manifest_json || !out_digest_sha256 || !out_dependency_count) {
        dependency_diagnostics(diagnostics, diagnostics_size, "dependency collector arguments missing");
        return false;
    }
    *out_manifest_json = NULL;
    *out_dependency_count = 0u;
    out_digest_sha256[0] = '\0';
    dependency_diagnostics(diagnostics, diagnostics_size, NULL);

    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (!object->isDeleted && object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE &&
            Layout_ObjectStore_ValidateObject(object)) {
            ++capacity;
        }
    }
    if (capacity > 0u) {
        collected = (CoreSceneCompileDependency*)core_calloc(capacity, sizeof(*collected));
        digests = (char (*)[CORE_SCENE_COMPILE_SHA256_HEX_SIZE])
            core_calloc(capacity, sizeof(*digests));
        if (!collected || !digests) {
            core_free(digests);
            core_free(collected);
            dependency_diagnostics(diagnostics, diagnostics_size, "failed to allocate dependency collector");
            return false;
        }
    }
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        char resolved_path[512];
        CoreBuffer contents = {0};
        CoreResult read_result;
        size_t existing = count;
        if (object->isDeleted || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE ||
            !Layout_ObjectStore_ValidateObject(object)) {
            continue;
        }
        if (Layout_MeshAssetResolveRuntimePath(object->meshInstance.runtimePath,
                                               resolved_path,
                                               sizeof(resolved_path)) == LAYOUT_MESH_PATH_MISSING) {
            core_free(digests);
            core_free(collected);
            dependency_diagnostics(diagnostics, diagnostics_size,
                                   "runtime mesh dependency could not be resolved");
            return false;
        }
        read_result = core_io_read_all(resolved_path, &contents);
        if (read_result.code != CORE_OK || (!contents.data && contents.size > 0u) ||
            core_scene_compile_sha256(contents.data,
                                      contents.size,
                                      digests[count]).code != CORE_OK) {
            core_io_buffer_free(&contents);
            core_free(digests);
            core_free(collected);
            dependency_diagnostics(diagnostics, diagnostics_size,
                                   "runtime mesh dependency could not be read or digested");
            return false;
        }
        for (size_t j = 0u; j < count; ++j) {
            if (strcmp(collected[j].kind, "mesh_asset_runtime") == 0 &&
                strcmp(collected[j].identity, object->meshInstance.assetId) == 0) {
                existing = j;
                break;
            }
        }
        if (existing < count) {
            if (collected[existing].content_bytes != contents.size ||
                strcmp(digests[existing], digests[count]) != 0) {
                core_io_buffer_free(&contents);
                core_free(digests);
                core_free(collected);
                dependency_diagnostics(diagnostics, diagnostics_size,
                                       "one mesh dependency identity resolves to conflicting content");
                return false;
            }
            core_io_buffer_free(&contents);
            continue;
        }
        collected[count].kind = "mesh_asset_runtime";
        collected[count].identity = object->meshInstance.assetId;
        collected[count].content_sha256 = digests[count];
        collected[count].content_bytes = contents.size;
        ++count;
        core_io_buffer_free(&contents);
    }
    manifest_result = core_scene_compile_dependency_manifest_build(
        count > 0u ? collected : NULL,
        count,
        out_manifest_json,
        out_digest_sha256,
        diagnostics,
        diagnostics_size);
    core_free(digests);
    core_free(collected);
    if (manifest_result.code != CORE_OK) return false;
    *out_dependency_count = count;
    return true;
}
