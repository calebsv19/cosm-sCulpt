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
    const char* scene_authoring_path,
    LineDrawingSceneExportDependencies* out_dependencies,
    char* diagnostics,
    size_t diagnostics_size) {
    size_t capacity = 0u;
    size_t count = 0u;
    CoreResult manifest_result;

    if (!layout || !out_dependencies) {
        dependency_diagnostics(diagnostics, diagnostics_size, "dependency collector arguments missing");
        return false;
    }
    memset(out_dependencies, 0, sizeof(*out_dependencies));
    dependency_diagnostics(diagnostics, diagnostics_size, NULL);

    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (!object->isDeleted && object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE &&
            Layout_ObjectStore_ValidateObject(object)) {
            ++capacity;
        }
    }
    if (capacity > 0u) {
        out_dependencies->entries = (CoreSceneCompileDependency*)
            core_calloc(capacity, sizeof(*out_dependencies->entries));
        out_dependencies->payload_buffers = (CoreBuffer*)
            core_calloc(capacity, sizeof(*out_dependencies->payload_buffers));
        out_dependencies->content_digests = (char (*)[CORE_SCENE_COMPILE_SHA256_HEX_SIZE])
            core_calloc(capacity, sizeof(*out_dependencies->content_digests));
        if (!out_dependencies->entries || !out_dependencies->payload_buffers ||
            !out_dependencies->content_digests) {
            LineDrawingSceneExportDependencies_Destroy(out_dependencies);
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
        if (Layout_MeshAssetResolveRuntimePathForScene(object->meshInstance.runtimePath,
                                                       scene_authoring_path,
                                                       resolved_path,
                                                       sizeof(resolved_path)) == LAYOUT_MESH_PATH_MISSING) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     "runtime mesh dependency missing: object=%s asset=%s path=%s",
                     object->coreMeta.object_id,
                     object->meshInstance.assetId,
                     object->meshInstance.runtimePath);
            LineDrawingSceneExportDependencies_Destroy(out_dependencies);
            dependency_diagnostics(diagnostics, diagnostics_size, message);
            return false;
        }
        read_result = core_io_read_all(resolved_path, &contents);
        if (read_result.code != CORE_OK || (!contents.data && contents.size > 0u) ||
            core_scene_compile_sha256(contents.data,
                                      contents.size,
                                      out_dependencies->content_digests[count]).code != CORE_OK) {
            core_io_buffer_free(&contents);
            char message[512];
            snprintf(message,
                     sizeof(message),
                     "runtime mesh dependency unreadable: object=%s asset=%s path=%s",
                     object->coreMeta.object_id,
                     object->meshInstance.assetId,
                     resolved_path);
            LineDrawingSceneExportDependencies_Destroy(out_dependencies);
            dependency_diagnostics(diagnostics, diagnostics_size, message);
            return false;
        }
        for (size_t j = 0u; j < count; ++j) {
            if (strcmp(out_dependencies->entries[j].kind, "mesh_asset_runtime") == 0 &&
                strcmp(out_dependencies->entries[j].identity, object->meshInstance.assetId) == 0) {
                existing = j;
                break;
            }
        }
        if (existing < count) {
            if (out_dependencies->entries[existing].content_bytes != contents.size ||
                strcmp(out_dependencies->content_digests[existing],
                       out_dependencies->content_digests[count]) != 0) {
                core_io_buffer_free(&contents);
                LineDrawingSceneExportDependencies_Destroy(out_dependencies);
                dependency_diagnostics(diagnostics, diagnostics_size,
                                       "one mesh dependency identity resolves to conflicting content");
                return false;
            }
            core_io_buffer_free(&contents);
            continue;
        }
        out_dependencies->entries[count].kind = "mesh_asset_runtime";
        out_dependencies->entries[count].identity = object->meshInstance.assetId;
        out_dependencies->entries[count].content_sha256 = out_dependencies->content_digests[count];
        out_dependencies->entries[count].content_bytes = contents.size;
        out_dependencies->entries[count].payload_data = contents.data;
        out_dependencies->payload_buffers[count] = contents;
        ++count;
        out_dependencies->count = count;
    }
    manifest_result = core_scene_compile_dependency_manifest_build(
        count > 0u ? out_dependencies->entries : NULL,
        count,
        &out_dependencies->manifest_json,
        out_dependencies->digest_sha256,
        diagnostics,
        diagnostics_size);
    if (manifest_result.code != CORE_OK) {
        LineDrawingSceneExportDependencies_Destroy(out_dependencies);
        return false;
    }
    return true;
}

void LineDrawingSceneExportDependencies_Destroy(
    LineDrawingSceneExportDependencies* dependencies) {
    if (!dependencies) return;
    if (dependencies->payload_buffers) {
        for (size_t i = 0u; i < dependencies->count; ++i) {
            core_io_buffer_free(&dependencies->payload_buffers[i]);
        }
    }
    core_free(dependencies->manifest_json);
    core_free(dependencies->content_digests);
    core_free(dependencies->payload_buffers);
    core_free(dependencies->entries);
    memset(dependencies, 0, sizeof(*dependencies));
}
